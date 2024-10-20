/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package controlplane

import (
	"context"
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

func (a *API) CreateEndpoints(r *mux.Router) {
	r.HandleFunc("/media-proxy", a.ListMediaProxies).Methods("GET")
	r.HandleFunc("/media-proxy/{id}", a.GetMediaProxy).Methods("GET")
	r.HandleFunc("/media-proxy", a.AddMediaProxy).Methods("PUT")            // for DEBUG purposes, TODO: remove
	r.HandleFunc("/media-proxy/{id}", a.DeleteMediaProxy).Methods("DELETE") // for DEBUG purposes, TODO: remove

	r.HandleFunc("/connection", a.ListConnections).Methods("GET")
	r.HandleFunc("/connection/{id}", a.GetConnection).Methods("GET")
	r.HandleFunc("/connection", a.AddConnection).Methods("PUT")            // for DEBUG purposes, TODO: remove
	r.HandleFunc("/connection/{id}", a.DeleteConnection).Methods("DELETE") // for DEBUG purposes, TODO: remove

	r.HandleFunc("/multipoint-group", a.ListMultipointGroups).Methods("GET")
	r.HandleFunc("/multipoint-group/{id}", a.GetMultipointGroup).Methods("GET")
	r.HandleFunc("/multipoint-group", a.AddMultipointGroup).Methods("PUT")
	r.HandleFunc("/multipoint-group/{id}", a.DeleteMultipointGroup).Methods("DELETE")

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
