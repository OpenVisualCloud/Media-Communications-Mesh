package logic_test

import (
	"context"
	"sync"
	"testing"

	"github.com/stretchr/testify/require"

	"control-plane-agent/internal/event"
	"control-plane-agent/internal/logic"
	"control-plane-agent/internal/logic/actions"
	"control-plane-agent/internal/model"
)

type MockedAction_TestEventsTriggerAction struct{}

func (a *MockedAction_TestEventsTriggerAction) ValidateModifier(modifier string) error {
	return nil
}

func (a *MockedAction_TestEventsTriggerAction) Perform(ctx context.Context, modifier string, param event.Params) (context.Context, bool, error) {
	return context.WithValue(ctx, event.ParamName("out_param"), modifier), true, nil
}

func TestEventsTriggerAction(t *testing.T) {
	cases := []struct {
		evt      event.Type
		outParam string
	}{
		{
			evt:      event.OnRegisterMediaProxy,
			outParam: "1",
		},
		{
			evt:      event.OnRegisterMediaProxyOk,
			outParam: "2",
		},
		{
			evt:      event.OnActivateMediaProxy,
			outParam: "3",
		},
		{
			evt:      event.OnUnregisterMediaProxy,
			outParam: "4",
		},
		{
			evt:      event.OnUnregisterMediaProxyOk,
			outParam: "5",
		},
		{
			evt:      event.OnRegisterConnection,
			outParam: "6",
		},
		{
			evt:      event.OnRegisterConnectionOk,
			outParam: "7",
		},
		{
			evt:      event.OnUnregisterConnection,
			outParam: "8",
		},
		{
			evt:      event.OnUnregisterConnectionOk,
			outParam: "9",
		},
		{
			evt:      event.OnMultipointGroupAdded,
			outParam: "10",
		},
		{
			evt:      event.OnMultipointGroupUpdated,
			outParam: "11",
		},
		{
			evt:      event.OnMultipointGroupDeleted,
			outParam: "12",
		},
		{
			evt:      event.OnProxyDisconnected,
			outParam: "13",
		},
	}

	manifest := `
logic:
  on-register-proxy:
    action(1):

  on-register-proxy-ok:
    action(2):

  on-activate-proxy:
    action(3):

  on-unregister-proxy:
    action(4):

  on-unregister-proxy-ok:
    action(5):

  on-register-connection:
    action(6):

  on-register-connection-ok:
    action(7):

  on-unregister-connection:
    action(8):

  on-unregister-connection-ok:
    action(9):

  on-multipoint-group-added:
    action(10):

  on-multipoint-group-updated:
    action(11):

  on-multipoint-group-deleted:
    action(12):

  on-proxy-disconnected:
    action(13):
`

	err := logic.LogicController.Init()
	require.NoError(t, err)

	actions.RegisterAction("action", &MockedAction_TestEventsTriggerAction{})

	err = logic.LogicController.ParseManifest(manifest)
	require.NoError(t, err)

	err = event.EventProcessor.Init(event.EventProcessorConfig{EventHandler: &logic.LogicController})
	require.NoError(t, err)

	ctx, cancel := context.WithCancel(context.Background())

	var wg sync.WaitGroup

	wg.Add(1)
	go func() {
		defer wg.Done()
		event.EventProcessor.Run(ctx)
		cancel()
	}()

	for _, v := range cases {
		reply := event.PostEventSync(ctx, v.evt, nil)
		require.NoError(t, reply.Err)

		outParam, ok := reply.Ctx.Value(event.ParamName("out_param")).(string)
		require.True(t, ok)
		require.Equal(t, v.outParam, outParam)
	}

	cancel()
	wg.Wait()
}

type MockedAction_TestEventParams struct{}

func (a *MockedAction_TestEventParams) ValidateModifier(modifier string) error {
	return nil
}

func (a *MockedAction_TestEventParams) Perform(ctx context.Context, modifier string, param event.Params) (context.Context, bool, error) {
	str, err := param.GetString("string")
	if err == nil {
		ctx = context.WithValue(ctx, event.ParamName("string"), str)
	}
	n, err := param.GetUint32("uint32")
	if err == nil {
		ctx = context.WithValue(ctx, event.ParamName("uint32"), n)
	}
	cfg, err := param.GetSDKConnConfig("conn_cfg")
	if err == nil {
		ctx = context.WithValue(ctx, event.ParamName("conn_cfg"), cfg)
	}
	return ctx, true, nil
}

func TestEventParams(t *testing.T) {
	cases := []struct {
		params map[string]interface{}
	}{
		{
			params: map[string]interface{}{},
		},
		{
			params: map[string]interface{}{
				"string": "123",
			},
		},
		{
			params: map[string]interface{}{
				"uint32": uint32(123),
			},
		},
		{
			params: map[string]interface{}{
				"conn_cfg": &model.SDKConnectionConfig{
					BufQueueCapacity: 123,
					MaxPayloadSize:   456,
					MaxMetadataSize:  789,
				},
			},
		},
		{
			params: map[string]interface{}{
				"string": "234",
				"uint32": uint32(345),
				"conn_cfg": &model.SDKConnectionConfig{
					BufQueueCapacity: 987,
					MaxPayloadSize:   654,
					MaxMetadataSize:  321,
				},
			},
		},
	}

	manifest := `
logic:
  on-register-proxy:
    action:
`
	err := logic.LogicController.Init()
	require.NoError(t, err)

	actions.RegisterAction("action", &MockedAction_TestEventParams{})

	err = logic.LogicController.ParseManifest(manifest)
	require.NoError(t, err)

	err = event.EventProcessor.Init(event.EventProcessorConfig{EventHandler: &logic.LogicController})
	require.NoError(t, err)

	ctx, cancel := context.WithCancel(context.Background())

	var wg sync.WaitGroup

	wg.Add(1)
	go func() {
		defer wg.Done()
		event.EventProcessor.Run(ctx)
		cancel()
	}()

	for _, v := range cases {
		reply := event.PostEventSync(ctx, event.OnRegisterMediaProxy, v.params)
		require.NoError(t, reply.Err)

		outParams := map[string]interface{}{}

		str, ok := reply.Ctx.Value(event.ParamName("string")).(string)
		if ok {
			outParams["string"] = str
		}
		n, ok := reply.Ctx.Value(event.ParamName("uint32")).(uint32)
		if ok {
			outParams["uint32"] = n
		}
		connCfg, ok := reply.Ctx.Value(event.ParamName("conn_cfg")).(*model.SDKConnectionConfig)
		if ok {
			outParams["conn_cfg"] = connCfg
		}

		require.Equal(t, v.params, outParams)
	}

	cancel()
	wg.Wait()
}

type MockedAction_TestActionChain struct{}

func (a *MockedAction_TestActionChain) ValidateModifier(modifier string) error {
	return nil
}

func (a *MockedAction_TestActionChain) Perform(ctx context.Context, modifier string, param event.Params) (context.Context, bool, error) {
	str, err := param.GetString("in-" + modifier)
	if err == nil {
		ctx = context.WithValue(ctx, event.ParamName("out-"+modifier), str)
	}
	return ctx, true, nil
}

func TestActionChain(t *testing.T) {
	cases := []struct {
		params    map[string]interface{}
		outParams map[string]interface{}
	}{
		{
			params:    map[string]interface{}{},
			outParams: map[string]interface{}{},
		},
		{
			params: map[string]interface{}{
				"in-1": "123",
				"in-2": "abc",
				"in-3": "ABC789",
			},
			outParams: map[string]interface{}{
				"out-1": "123",
				"out-2": "abc",
				"out-3": "ABC789",
			},
		},
	}

	manifest := `
logic:
  on-register-proxy:
    action(1):
      success:
        action(2):
          true:  
            action(3):
`
	err := logic.LogicController.Init()
	require.NoError(t, err)

	actions.RegisterAction("action", &MockedAction_TestActionChain{})

	err = logic.LogicController.ParseManifest(manifest)
	require.NoError(t, err)

	err = event.EventProcessor.Init(event.EventProcessorConfig{EventHandler: &logic.LogicController})
	require.NoError(t, err)

	ctx, cancel := context.WithCancel(context.Background())

	var wg sync.WaitGroup

	wg.Add(1)
	go func() {
		defer wg.Done()
		event.EventProcessor.Run(ctx)
		cancel()
	}()

	for _, v := range cases {
		reply := event.PostEventSync(ctx, event.OnRegisterMediaProxy, v.params)
		require.NoError(t, reply.Err)

		outParams := map[string]interface{}{}

		for k := range v.outParams {
			str, ok := reply.Ctx.Value(event.ParamName(k)).(string)
			if ok {
				outParams[k] = str
			}
		}

		require.Equal(t, v.outParams, outParams)
	}

	cancel()
	wg.Wait()
}
