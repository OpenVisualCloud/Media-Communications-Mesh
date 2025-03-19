package mesh

import (
	"context"
	"control-plane-agent/internal/model"
	"control-plane-agent/internal/registry"
	"errors"
)

func CheckIfGroupAcceptsConnectionKind(ctx context.Context, group *model.MultipointGroup, kind string) error {
	if kind != "rx" {
		return nil
	}

	conns, err := registry.ConnRegistry.List(ctx, group.ConnIds, false, true)
	if err != nil {
		return nil
	}

	for i := range conns {
		if conns[i].Config != nil && conns[i].Config.Kind == "rx" {
			return errors.New("rx conn already linked in group")
		}
	}

	bridges, err := registry.BridgeRegistry.List(ctx, group.BridgeIds, false, true)
	if err != nil {
		return nil
	}

	for i := range bridges {
		if bridges[i].Config != nil && bridges[i].Config.Kind == "rx" {
			return errors.New("rx bridge already linked in group")
		}
	}

	return nil
}
