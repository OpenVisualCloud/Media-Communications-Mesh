package mesh

import (
	"context"
	"errors"

	"github.com/sirupsen/logrus"

	pb "control-plane-agent/api/proxy/proto/mediaproxy"
	"control-plane-agent/api/proxy/proto/sdk"
	"control-plane-agent/internal/model"
	"control-plane-agent/internal/registry"
	"control-plane-agent/internal/utils"
)

func ApplyProxyConfig(ctx context.Context, mp *model.MediaProxy, groups []model.MultipointGroup) error {

	pbGroups := []*pb.MultipointGroup{}
	for i := range groups {
		connIds := utils.Intersection(groups[i].ConnIds, mp.ConnIds)
		if len(connIds) == 0 {
			continue
		}
		pbGroups = append(pbGroups, &pb.MultipointGroup{
			GroupId:   groups[i].Id + "/" + mp.Id, // composite value: Group ID + Proxy ID
			ConnIds:   connIds,
			BridgeIds: utils.Intersection(groups[i].BridgeIds, mp.BridgeIds),
		})
	}

	pbBridges := make([]*pb.Bridge, 0, len(mp.BridgeIds))
	for _, bridgeId := range mp.BridgeIds {
		bridge, err := registry.BridgeRegistry.Get(ctx, bridgeId, true)
		if err != nil {
			logrus.Warnf("Apply Config bridge registry err: %v, id '%v'", err, bridgeId)
			continue
		}
		err = bridge.ValidateConfig()
		if err != nil {
			logrus.Errorf("Apply Config bridge validate cfg err: %v", err)
			continue
		}

		connConfig := &sdk.ConnectionConfig{}
		bridge.Config.AssignToPb(connConfig)

		pbBridge := &pb.Bridge{
			BridgeId:   bridge.Id,
			Type:       bridge.Config.Type,
			Kind:       bridge.Config.Kind,
			ConnConfig: connConfig,
		}

		switch bridge.Config.Type {
		case "st2110":
			pbBridge.Config = &pb.Bridge_St2110{
				St2110: &pb.BridgeST2110{
					RemoteIp:  bridge.Config.ST2110.RemoteIP,
					Port:      uint32(bridge.Config.ST2110.Port),
					Transport: bridge.Config.ST2110.Transport,
				},
			}
		case "rdma":
			pbBridge.Config = &pb.Bridge_Rdma{
				Rdma: &pb.BridgeRDMA{
					RemoteIp: bridge.Config.RDMA.RemoteIP,
					Port:     uint32(bridge.Config.RDMA.Port),
				},
			}
		}

		pbBridges = append(pbBridges, pbBridge)

		// // DEBUG
		// if i&1 == 0 {
		// 	pbBridges[i] = &pb.Bridge{
		// 		BridgeId: bridgeId,
		// 		Type:     "st2110",
		// 		Kind:     "tx",
		// 		Config: &pb.Bridge_St2110{
		// 			St2110: &pb.BridgeST2110{
		// 				RemoteIp:  "192.168.1.10",
		// 				Port:      9009,
		// 				Transport: "20",
		// 			},
		// 		},
		// 	}
		// } else {
		// 	pbBridges[i] = &pb.Bridge{
		// 		BridgeId: bridgeId,
		// 		Type:     "rdma",
		// 		Kind:     "rx",
		// 		Config: &pb.Bridge_Rdma{
		// 			Rdma: &pb.BridgeRDMA{
		// 				RemoteIp: "192.168.200.1",
		// 				Port:     9010,
		// 			},
		// 		},
		// 	}
		// }
		// // DEBUG
	}

	err := mp.SendApplyConfigCommand(ctx, &pb.ApplyConfigRequest{
		Groups:  pbGroups,
		Bridges: pbBridges,
	})
	if errors.Is(err, model.ErrProxyNotReady) {
		return nil
	}
	return err
}
