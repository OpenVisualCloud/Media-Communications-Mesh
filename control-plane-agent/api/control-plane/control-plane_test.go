package controlplane

import (
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"sync"
	"testing"

	"github.com/gorilla/mux"
	"github.com/stretchr/testify/require"

	"control-plane-agent/internal/event"
	"control-plane-agent/internal/logic"
	"control-plane-agent/internal/model"
	"control-plane-agent/internal/registry"
)

func TestEndpoints(t *testing.T) {
	event.EventProcessor.Init(event.EventProcessorConfig{EventHandler: &logic.LogicController})
	registry.MediaProxyRegistry.Init(registry.MediaProxyRegistryConfig{})
	registry.ConnRegistry.Init()
	registry.MultipointGroupRegistry.Init()
	registry.BridgeRegistry.Init()

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	var wg sync.WaitGroup

	wg.Add(1)
	go func() {
		defer wg.Done()
		event.EventProcessor.Run(ctx)
		cancel()
	}()

	wg.Add(1)
	go func() {
		defer wg.Done()
		err := registry.MediaProxyRegistry.Run(ctx)
		require.NoError(t, err)
	}()

	wg.Add(1)
	go func() {
		defer wg.Done()
		err := registry.ConnRegistry.Run(ctx)
		require.NoError(t, err)
	}()

	wg.Add(1)
	go func() {
		defer wg.Done()
		err := registry.MultipointGroupRegistry.Run(ctx)
		require.NoError(t, err)
	}()

	wg.Add(1)
	go func() {
		defer wg.Done()
		err := registry.BridgeRegistry.Run(ctx)
		require.NoError(t, err)
	}()

	api := NewAPI(Config{})
	r := mux.NewRouter()
	api.CreateEndpoints(r)

	rr := httptest.NewRecorder()

	var err error
	var decoder *json.Decoder
	var req *http.Request
	var proxyId string
	var connId string
	var bridgeId string
	var groupId string

	cases := []struct {
		fn func()
	}{
		//---------------------------------
		// List media proxies - Empty list
		//---------------------------------
		{
			fn: func() {
				req, _ := http.NewRequest("GET", "/media-proxy", nil)

				r.ServeHTTP(rr, req)
				require.Equal(t, http.StatusOK, rr.Code)

				var resp struct {
					MediaProxy []model.MediaProxy `json:"mediaProxy"`
				}
				decoder = json.NewDecoder(rr.Body)
				decoder.DisallowUnknownFields()
				err = decoder.Decode(&resp)
				require.NoError(t, err)

				require.Equal(t, 0, len(resp.MediaProxy))
			},
		},
		//--------------------------------------
		// List media proxies - One media proxy
		//--------------------------------------
		{
			fn: func() {
				proxyId, _ = registry.MediaProxyRegistry.Add(ctx, model.MediaProxy{
					Config: &model.MediaProxyConfig{},
					Status: &model.MediaProxyStatus{},
				})

				req, _ = http.NewRequest("GET", "/media-proxy", nil)

				r.ServeHTTP(rr, req)
				require.Equal(t, http.StatusOK, rr.Code)

				var resp struct {
					MediaProxy []model.MediaProxy `json:"mediaProxy"`
				}
				decoder = json.NewDecoder(rr.Body)
				decoder.DisallowUnknownFields()
				err = decoder.Decode(&resp)
				require.NoError(t, err)

				require.Equal(t, 1, len(resp.MediaProxy))
				require.Equal(t, proxyId, resp.MediaProxy[0].Id)
				require.Equal(t, (*model.MediaProxyConfig)(nil), resp.MediaProxy[0].Config)
				require.Equal(t, (*model.MediaProxyStatus)(nil), resp.MediaProxy[0].Status)
				require.Equal(t, 0, len(resp.MediaProxy[0].Conns))
				require.Equal(t, 0, len(resp.MediaProxy[0].Bridges))
			},
		},
		//------------------------------------------------------------------
		// List media proxies - One media proxy, one connection, one bridge
		//------------------------------------------------------------------
		{
			fn: func() {
				connId, _ = registry.ConnRegistry.Add(ctx, model.Connection{})
				registry.MediaProxyRegistry.Update_LinkConn(ctx, proxyId, connId)

				bridgeId, _ = registry.BridgeRegistry.Add(ctx, model.Bridge{})
				registry.MediaProxyRegistry.Update_LinkBridge(ctx, proxyId, bridgeId)

				req, _ = http.NewRequest("GET", "/media-proxy?config&status", nil)

				r.ServeHTTP(rr, req)
				require.Equal(t, http.StatusOK, rr.Code)

				var resp struct {
					MediaProxy []model.MediaProxy `json:"mediaProxy"`
				}
				decoder = json.NewDecoder(rr.Body)
				decoder.DisallowUnknownFields()
				err = decoder.Decode(&resp)
				require.NoError(t, err)

				require.Equal(t, 1, len(resp.MediaProxy))
				require.Equal(t, proxyId, resp.MediaProxy[0].Id)
				require.NotEqual(t, (*model.MediaProxyConfig)(nil), resp.MediaProxy[0].Config)
				require.NotEqual(t, (*model.MediaProxyStatus)(nil), resp.MediaProxy[0].Status)
				require.Equal(t, 1, len(resp.MediaProxy[0].Conns))
				require.Equal(t, connId, resp.MediaProxy[0].Conns[0].Id)
				require.Equal(t, 1, len(resp.MediaProxy[0].Bridges))
				require.Equal(t, bridgeId, resp.MediaProxy[0].Bridges[0].Id)
			},
		},
		//-------------------------------------
		// List multipoint groups - Empty list
		//-------------------------------------
		{
			fn: func() {
				req, _ := http.NewRequest("GET", "/multipoint-group", nil)

				r.ServeHTTP(rr, req)
				require.Equal(t, http.StatusOK, rr.Code)

				var resp struct {
					MultipointGroup []model.MultipointGroup `json:"multipointGroup"`
				}
				decoder = json.NewDecoder(rr.Body)
				decoder.DisallowUnknownFields()
				err = decoder.Decode(&resp)
				require.NoError(t, err)

				require.Equal(t, 0, len(resp.MultipointGroup))
			},
		},
		//-----------------------------------------------
		// List multipoint groups - One multipoint group
		//-----------------------------------------------
		{
			fn: func() {
				groupId, _ = registry.MultipointGroupRegistry.Add(ctx, model.MultipointGroup{
					Config: &model.MultipointGroupConfig{},
					Status: &model.ConnectionStatus{},
				})

				req, _ := http.NewRequest("GET", "/multipoint-group", nil)

				r.ServeHTTP(rr, req)
				require.Equal(t, http.StatusOK, rr.Code)

				var resp struct {
					MultipointGroup []model.MultipointGroup `json:"multipointGroup"`
				}
				decoder = json.NewDecoder(rr.Body)
				decoder.DisallowUnknownFields()
				err = decoder.Decode(&resp)
				require.NoError(t, err)

				require.Equal(t, 1, len(resp.MultipointGroup))
				require.Equal(t, groupId, resp.MultipointGroup[0].Id)
				require.Equal(t, (*model.MultipointGroupConfig)(nil), resp.MultipointGroup[0].Config)
				require.Equal(t, (*model.ConnectionStatus)(nil), resp.MultipointGroup[0].Status)
				require.Equal(t, 0, len(resp.MultipointGroup[0].ConnIds))
				require.Equal(t, 0, len(resp.MultipointGroup[0].BridgeIds))
			},
		},
		//---------------------------------------------------------------------------
		// List multipoint groups - One multipoint group, one connection, one bridge
		//---------------------------------------------------------------------------
		{
			fn: func() {
				registry.MultipointGroupRegistry.Update_LinkConn(ctx, groupId, connId)
				registry.MultipointGroupRegistry.Update_LinkBridge(ctx, groupId, bridgeId)

				req, _ := http.NewRequest("GET", "/multipoint-group?config&status", nil)

				r.ServeHTTP(rr, req)
				require.Equal(t, http.StatusOK, rr.Code)

				var resp struct {
					MultipointGroup []model.MultipointGroup `json:"multipointGroup"`
				}
				decoder = json.NewDecoder(rr.Body)
				decoder.DisallowUnknownFields()
				err = decoder.Decode(&resp)
				require.NoError(t, err)

				require.Equal(t, 1, len(resp.MultipointGroup))
				require.Equal(t, groupId, resp.MultipointGroup[0].Id)
				require.NotEqual(t, (*model.MultipointGroupConfig)(nil), resp.MultipointGroup[0].Config)
				require.NotEqual(t, (*model.ConnectionStatus)(nil), resp.MultipointGroup[0].Status)
				require.Equal(t, 1, len(resp.MultipointGroup[0].ConnIds))
				require.Equal(t, connId, resp.MultipointGroup[0].ConnIds[0])
				require.Equal(t, 1, len(resp.MultipointGroup[0].BridgeIds))
				require.Equal(t, bridgeId, resp.MultipointGroup[0].BridgeIds[0])
			},
		},
	}

	for i := range cases {
		cases[i].fn()
	}

	cancel()

	wg.Wait()
}
