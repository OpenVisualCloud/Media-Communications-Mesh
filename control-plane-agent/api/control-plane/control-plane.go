/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package controlplane

import (
	"context"
	"control-plane-agent/internal/logic/mesh"
	"control-plane-agent/internal/registry"
	"errors"
	"fmt"
	"net/http"
	"time"

	"github.com/gorilla/handlers"
	"github.com/gorilla/mux"
	"github.com/sirupsen/logrus"
)

type Config struct {
	ListenPort int
}

type API struct {
	cfg Config
	ctx context.Context
	srv *http.Server
}

func NewAPI(cfg Config) *API {
	return &API{
		cfg: cfg,
	}
}

// DEBUG
func (a *API) DEBUG_MediaProxy_SendCommand(w http.ResponseWriter, r *http.Request) {
	id := mux.Vars(r)["id"]
	cmd := mux.Vars(r)["cmd"]

	proxy, err := registry.MediaProxyRegistry.Get(r.Context(), id, false)
	if errors.Is(err, registry.ErrResourceNotFound) {
		http.Error(w, err.Error(), http.StatusNotFound)
		return
	}
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}

	if cmd == "apply-config" {
		err := mesh.ApplyProxyConfig(r.Context(), &proxy, nil)
		if err != nil {
			http.Error(w, err.Error(), http.StatusInternalServerError)
		}
		return
	}

	res, err := proxy.ExecDebugCommand(r.Context(), cmd)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}

	fmt.Fprintln(w, res)
}

func (a *API) MultipointGroup_AddConn(w http.ResponseWriter, r *http.Request) {
	id := mux.Vars(r)["id"]
	connId := mux.Vars(r)["connId"]

	// Add connection to Multipoint Group
	err := registry.MultipointGroupRegistry.Update_LinkConn(r.Context(), id, connId)
	if errors.Is(err, registry.ErrResourceNotFound) {
		http.Error(w, err.Error(), http.StatusNotFound)
		return
	}
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}
}

func (a *API) MultipointGroup_AddBridge(w http.ResponseWriter, r *http.Request) {
	id := mux.Vars(r)["id"]
	bridgeId := mux.Vars(r)["bridgeId"]

	// Add bridge to Multipoint Group
	err := registry.MultipointGroupRegistry.Update_LinkBridge(r.Context(), id, bridgeId)
	if errors.Is(err, registry.ErrResourceNotFound) {
		http.Error(w, err.Error(), http.StatusNotFound)
		return
	}
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}
}

func (a *API) MediaProxy_AddBridge(w http.ResponseWriter, r *http.Request) {
	id := mux.Vars(r)["id"]
	bridgeId := mux.Vars(r)["bridgeId"]

	// Add bridge to Media Proxy
	err := registry.MediaProxyRegistry.Update_LinkBridge(r.Context(), id, bridgeId)
	if errors.Is(err, registry.ErrResourceNotFound) {
		http.Error(w, err.Error(), http.StatusNotFound)
		return
	}
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}
}

// DEBUG

func (a *API) CreateEndpoints(r *mux.Router) {
	r.HandleFunc("/media-proxy", a.ListMediaProxies).Methods("GET")
	r.HandleFunc("/media-proxy/{id}", a.GetMediaProxy).Methods("GET")
	r.HandleFunc("/media-proxy", a.AddMediaProxy).Methods("PUT")            // for DEBUG purposes, TODO: remove
	r.HandleFunc("/media-proxy/{id}", a.DeleteMediaProxy).Methods("DELETE") // for DEBUG purposes, TODO: remove

	// DEBUG
	r.HandleFunc("/media-proxy/{id}/command/{cmd}", a.DEBUG_MediaProxy_SendCommand).Methods("GET")
	// DEBUG

	r.HandleFunc("/connection", a.ListConnections).Methods("GET")
	r.HandleFunc("/connection/{id}", a.GetConnection).Methods("GET")
	r.HandleFunc("/connection", a.AddConnection).Methods("PUT")            // for DEBUG purposes, TODO: remove
	r.HandleFunc("/connection/{id}", a.DeleteConnection).Methods("DELETE") // for DEBUG purposes, TODO: remove

	r.HandleFunc("/multipoint-group", a.ListMultipointGroups).Methods("GET")
	r.HandleFunc("/multipoint-group/{id}", a.GetMultipointGroup).Methods("GET")
	r.HandleFunc("/multipoint-group", a.AddMultipointGroup).Methods("PUT")
	r.HandleFunc("/multipoint-group/{id}", a.DeleteMultipointGroup).Methods("DELETE")

	// DEBUG
	r.HandleFunc("/multipoint-group/{id}/add-conn/{connId}", a.MultipointGroup_AddConn).Methods("GET")
	r.HandleFunc("/multipoint-group/{id}/add-bridge/{bridgeId}", a.MultipointGroup_AddBridge).Methods("GET")
	r.HandleFunc("/media-proxy/{id}/add-bridge/{bridgeId}", a.MediaProxy_AddBridge).Methods("GET")
	// DEBUG

	r.HandleFunc("/bridge", a.ListBridges).Methods("GET")
	r.HandleFunc("/bridge/{id}", a.GetBridge).Methods("GET")
	r.HandleFunc("/bridge", a.AddBridge).Methods("PUT")
	r.HandleFunc("/bridge/{id}", a.DeleteBridge).Methods("DELETE")
}

func (a *API) Run(ctx context.Context) {
	a.ctx = ctx

	r := mux.NewRouter()
	a.CreateEndpoints(r)

	headersOk := handlers.AllowedHeaders([]string{"X-Requested-With"})
	originsOk := handlers.AllowedOrigins([]string{"*"})
	methodsOk := handlers.AllowedMethods([]string{"GET", "HEAD", "POST", "PUT", "OPTIONS"})

	a.srv = &http.Server{
		Handler: handlers.CORS(
			originsOk,
			headersOk,
			methodsOk,
			handlers.AllowCredentials(),
		)(handlers.CompressHandler(r)),
		Addr:         fmt.Sprintf(":%v", a.cfg.ListenPort),
		WriteTimeout: 5 * time.Second,
		ReadTimeout:  5 * time.Second,
	}

	done := make(chan interface{})
	exit := make(chan interface{})
	go func() {
		select {
		case <-ctx.Done():
		case <-exit:
		}
		cctx, ccancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer ccancel()
		a.srv.Shutdown(cctx)
		close(done)
	}()

	logrus.Infof("Server starts listening at :%v - Control Plane API (REST)", a.cfg.ListenPort)

	err := a.srv.ListenAndServe()
	if err != http.ErrServerClosed {
		logrus.Errorf("Control Plane API server err: %v", err)
	}

	close(exit)
	<-done
}

func applyContentTypeHeaderJSON(w http.ResponseWriter) {
	w.Header().Set("Content-Type", "application/json")
}
