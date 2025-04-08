package model

import (
	"context"
	"errors"
	"fmt"
	"time"

	pb "control-plane-agent/api/proxy/proto/mediaproxy"
	"control-plane-agent/internal/utils"

	"github.com/google/uuid"
	"github.com/sirupsen/logrus"
)

type MediaProxy struct {
	Id               string            `json:"id,omitempty"`
	Config           *MediaProxyConfig `json:"config,omitempty"`
	Status           *MediaProxyStatus `json:"status,omitempty"`
	ConnIds          []string          `json:"-"`       // array of local connection ids, hidden in JSON
	Conns            []Connection      `json:"conns"`   // array of local connections populated only in JSON
	BridgeIds        []string          `json:"-"`       // array of bridge ids, hidden in JSON
	Bridges          []Bridge          `json:"bridges"` // array of bridges populated only in JSON
	RDMAPortsAllowed PortMask          `json:"-"`

	Active bool `json:"-"`

	queue                    chan CommandRequest
	cancelCommandRequestFunc func(reqId string) // this is registered by Proxy API at proxy creation

	queueCtx       context.Context // this is used to stop processing command queue when proxy is about to be deleted
	cancelQueueCtx context.CancelFunc

	ReadyCh *utils.ReadinessChannel `json:"-"`
}

type ST2110ProxyConfig struct {
	DataplaneIPAddr string `json:"dataplaneIpAddr"`
	DevPortBDF      string `json:"devPort"`
}

type RDMAProxyConfig struct {
	DataplaneIPAddr     string `json:"dataplaneIpAddr"`
	DataplaneLocalPorts string `json:"dataplanePorts"`
}

type MediaProxyConfig struct {
	SDKAPIPort       uint32            `json:"sdkApiPort"`
	ControlplaneAddr string            `json:"controlplaneIpAddr"`
	ST2110           ST2110ProxyConfig `json:"st2110"`
	RDMA             RDMAProxyConfig   `json:"rdma"`
}

type MediaProxyStatus struct {
	Healthy      bool       `json:"healthy"`
	RegisteredAt CustomTime `json:"registeredAt"`
	ConnsNum     int        `json:"connsNum"`
	BridgesNum   int        `json:"bridgesNum"`
}

type CommandRequest struct {
	ProxyId   string
	PbRequest *pb.CommandRequest
	Reply     chan CommandReply
}

type CommandReply struct {
	Data interface{}
	Err  error
}

func (mp *MediaProxy) Init(cancelCommandRequestFunc func(reqId string)) {
	mp.queue = make(chan CommandRequest, 1000) // TODO: capacity to be configured
	mp.cancelCommandRequestFunc = cancelCommandRequestFunc

	ctx, cancel := context.WithCancel(context.Background())
	mp.queueCtx = ctx
	mp.cancelQueueCtx = cancel

	mp.ReadyCh = utils.NewReadinessChannel()
	go mp.ReadyCh.Run(ctx)
}

func (mp *MediaProxy) Deinit() {
	mp.cancelQueueCtx()
}

func (mp *MediaProxy) newCommandRequest() CommandRequest {
	return CommandRequest{
		ProxyId:   mp.Id,
		PbRequest: &pb.CommandRequest{ReqId: uuid.NewString()},
		Reply:     make(chan CommandReply, 1),
	}
}

func (mp *MediaProxy) GetNextCommandRequest(ctx context.Context) (CommandRequest, error) {
	select {
	case <-ctx.Done():
		return CommandRequest{}, ctx.Err()
	case <-mp.queueCtx.Done():
		return CommandRequest{}, errors.New("get next cmd: proxy cmd queue ctx cancelled")
	case req, ok := <-mp.queue:
		if !ok {
			return CommandRequest{}, errors.New("command queue closed")
		}
		return req, nil
	}
}

var ErrProxyNotReady = errors.New("proxy command stream not ready")

func (mp *MediaProxy) sendCommandSync(ctx context.Context, req CommandRequest) (reply CommandReply) {
	defer func() {
		if reply.Err != nil && mp.cancelCommandRequestFunc != nil {
			mp.cancelCommandRequestFunc(req.PbRequest.ReqId)
		}
	}()

	select {
	case <-ctx.Done():
		reply = CommandReply{Err: ctx.Err()}
		return
	case <-mp.ReadyCh.NotReady():
		reply = CommandReply{Err: ErrProxyNotReady}
		return
	case <-mp.queueCtx.Done():
		reply = CommandReply{Err: errors.New("send cmd: proxy cmd queue ctx cancelled")}
		return
	case mp.queue <- req:
	}

	cctx, cancel := context.WithTimeout(ctx, 10*time.Second)
	defer cancel()

	select {
	case <-cctx.Done():
		reply = CommandReply{Err: cctx.Err()}
	case <-mp.ReadyCh.NotReady():
		reply = CommandReply{Err: ErrProxyNotReady}
		return
	case <-mp.queueCtx.Done():
		reply = CommandReply{Err: errors.New("send cmd: proxy wait reply cmd queue ctx cancelled")}
	case reply = <-req.Reply:
	}
	return
}

func (mp *MediaProxy) ExecDebugCommand(ctx context.Context, text string) (string, error) {
	req := mp.newCommandRequest()
	req.PbRequest.Command = &pb.CommandRequest_Debug{
		Debug: &pb.DebugRequest{
			InText: text,
		},
	}
	logrus.Debugf("Send Debug Command %v", text)
	reply := mp.sendCommandSync(ctx, req)
	if reply.Err != nil {
		return "", fmt.Errorf("debug command err: %w", reply.Err)
	}
	resp, ok := reply.Data.(*pb.CommandReply_Debug)
	if !ok {
		return "", fmt.Errorf("debug command type case err")
	} else {
		logrus.Debugf("Debug command succeeded: %v", resp.Debug.OutText)
		return resp.Debug.OutText, nil
	}
}

func (mp *MediaProxy) SendApplyConfigCommand(ctx context.Context, config *pb.ApplyConfigRequest) error {
	select {
	case <-mp.queueCtx.Done():
		return nil
	default:
	}

	req := mp.newCommandRequest()
	req.PbRequest.Command = &pb.CommandRequest_ApplyConfig{ApplyConfig: config}

	// logrus.Debugf("Send ApplyConfig Command")
	reply := mp.sendCommandSync(ctx, req)
	if reply.Err != nil {
		return fmt.Errorf("apply config cmd err: %w", reply.Err)
	}
	_, ok := reply.Data.(*pb.CommandReply_ApplyConfig)
	if !ok {
		return fmt.Errorf("apply config cmd type case err")
	} else {
		// logrus.Debugf("ApplyConfig cmd succeeded")
		return nil
	}
}
