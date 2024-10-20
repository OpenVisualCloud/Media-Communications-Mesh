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

func (a *API) ListMultipointGroups(w http.ResponseWriter, r *http.Request) {
	query := r.URL.Query()
	addStatus := query.Has("status")
	addConfig := query.Has("config")

	items, err := registry.MultipointGroupRegistry.ListAll(r.Context(), addStatus, addConfig)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}

	resp := struct {
		MultipointGroup []model.MultipointGroup `json:"multipointGroup"`
	}{
		MultipointGroup: items,
	}
	applyContentTypeHeaderJSON(w)
	json.NewEncoder(w).Encode(resp)
}

func (a *API) GetMultipointGroup(w http.ResponseWriter, r *http.Request) {
	id := mux.Vars(r)["id"]
	addConfig := r.URL.Query().Has("config")

	item, err := registry.MultipointGroupRegistry.Get(r.Context(), id, addConfig)
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

func (a *API) AddMultipointGroup(w http.ResponseWriter, r *http.Request) {

	group := model.MultipointGroup{
		Status: &model.MultipointGroupStatus{},
		Config: &model.MultipointGroupConfig{},
	}

	id, err := registry.MultipointGroupRegistry.Add(r.Context(), group)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}

	group = model.MultipointGroup{Id: id}

	applyContentTypeHeaderJSON(w)
	json.NewEncoder(w).Encode(group)
}

func (a *API) DeleteMultipointGroup(w http.ResponseWriter, r *http.Request) {
	id := mux.Vars(r)["id"]

	err := registry.MultipointGroupRegistry.Delete(r.Context(), id)
	if errors.Is(err, registry.ErrResourceNotFound) {
		http.Error(w, err.Error(), http.StatusNotFound)
		return
	}
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}
}
