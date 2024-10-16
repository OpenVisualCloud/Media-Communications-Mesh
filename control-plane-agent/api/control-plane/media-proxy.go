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

func (a *API) ListMediaProxies(w http.ResponseWriter, r *http.Request) {
	query := r.URL.Query()
	addStatus := query.Has("status")
	addConfig := query.Has("config")

	items, err := registry.MediaProxyRegistry.ListAll(r.Context(), addStatus, addConfig)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}

	resp := struct {
		MediaProxy []model.MediaProxy `json:"mediaProxy"`
	}{
		MediaProxy: items,
	}
	applyContentTypeHeaderJSON(w)
	json.NewEncoder(w).Encode(resp)
}

func (a *API) GetMediaProxy(w http.ResponseWriter, r *http.Request) {
	id := mux.Vars(r)["id"]
	addConfig := r.URL.Query().Has("config")

	item, err := registry.MediaProxyRegistry.Get(r.Context(), id, addConfig)
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

func (a *API) AddMediaProxy(w http.ResponseWriter, r *http.Request) {

	proxy := model.MediaProxy{
		Status: &model.MediaProxyStatus{},
		Config: &model.MediaProxyConfig{},
	}

	id, err := registry.MediaProxyRegistry.Add(r.Context(), proxy)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}

	proxy = model.MediaProxy{Id: id}

	applyContentTypeHeaderJSON(w)
	json.NewEncoder(w).Encode(proxy)
}

func (a *API) DeleteMediaProxy(w http.ResponseWriter, r *http.Request) {
	id := mux.Vars(r)["id"]

	err := registry.MediaProxyRegistry.Delete(r.Context(), id)
	if errors.Is(err, registry.ErrResourceNotFound) {
		http.Error(w, err.Error(), http.StatusNotFound)
		return
	}
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}
}
