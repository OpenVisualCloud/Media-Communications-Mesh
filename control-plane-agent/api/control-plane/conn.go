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

func (a *API) ListConnections(w http.ResponseWriter, r *http.Request) {
	query := r.URL.Query()
	addStatus := query.Has("status")
	addConfig := query.Has("config")

	items, err := registry.ConnRegistry.ListAll(r.Context(), addStatus, addConfig)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}

	resp := struct {
		Conn []model.Connection `json:"connection"`
	}{
		Conn: items,
	}
	applyContentTypeHeaderJSON(w)
	json.NewEncoder(w).Encode(resp)
}

func (a *API) GetConnection(w http.ResponseWriter, r *http.Request) {
	id := mux.Vars(r)["id"]
	addConfig := r.URL.Query().Has("config")

	item, err := registry.ConnRegistry.Get(r.Context(), id, addConfig)
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

func (a *API) AddConnection(w http.ResponseWriter, r *http.Request) {

	conn := model.Connection{
		Status: &model.ConnectionStatus{},
		Config: &model.ConnectionConfig{
			Conn:    model.ConnectionST2110{},
			Payload: model.PayloadVideo{},
		},
	}

	id, err := registry.ConnRegistry.Add(r.Context(), conn)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}

	conn = model.Connection{Id: id}

	applyContentTypeHeaderJSON(w)
	json.NewEncoder(w).Encode(conn)
}

func (a *API) DeleteConnection(w http.ResponseWriter, r *http.Request) {
	id := mux.Vars(r)["id"]

	err := registry.ConnRegistry.Delete(r.Context(), id)
	if errors.Is(err, registry.ErrResourceNotFound) {
		http.Error(w, err.Error(), http.StatusNotFound)
		return
	}
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}
}
