/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package controlplane

import (
	"encoding/json"
	"errors"
	"net/http"

	"github.com/gorilla/mux"

	"control-plane-agent/internal/model"
	"control-plane-agent/internal/registry"
)

func (a *API) ListBridges(w http.ResponseWriter, r *http.Request) {
	query := r.URL.Query()
	addStatus := query.Has("status")
	addConfig := query.Has("config")

	items, err := registry.BridgeRegistry.List(r.Context(), nil, addStatus, addConfig)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}

	resp := struct {
		Bridge []model.Bridge `json:"bridge"`
	}{
		Bridge: items,
	}
	applyContentTypeHeaderJSON(w)
	json.NewEncoder(w).Encode(resp)
}

func (a *API) GetBridge(w http.ResponseWriter, r *http.Request) {
	id := mux.Vars(r)["id"]
	addConfig := r.URL.Query().Has("config")

	item, err := registry.BridgeRegistry.Get(r.Context(), id, addConfig)
	if errors.Is(err, registry.ErrResourceNotFound) {
		http.Error(w, err.Error(), http.StatusNotFound)
		return
	}
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}

	item.Id = "" // hide the id in JSON (omitempty)

	applyContentTypeHeaderJSON(w)
	json.NewEncoder(w).Encode(item)
}

func (a *API) AddBridge(w http.ResponseWriter, r *http.Request) {
	// DEBUG
	proxyId := r.URL.Query().Get("proxy")
	groupId := r.URL.Query().Get("group")
	// DEBUG

	bridge := model.Bridge{
		ProxyId: proxyId,
		GroupId: groupId,
		Config:  &model.BridgeConfig{},
		Status:  &model.ConnectionStatus{},
	}

	id, err := registry.BridgeRegistry.Add(r.Context(), bridge)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}

	bridge = model.Bridge{Id: id}

	applyContentTypeHeaderJSON(w)
	json.NewEncoder(w).Encode(bridge)
}

func (a *API) DeleteBridge(w http.ResponseWriter, r *http.Request) {
	id := mux.Vars(r)["id"]

	err := registry.BridgeRegistry.Delete(r.Context(), id)
	if errors.Is(err, registry.ErrResourceNotFound) {
		http.Error(w, err.Error(), http.StatusNotFound)
		return
	}
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}
}
