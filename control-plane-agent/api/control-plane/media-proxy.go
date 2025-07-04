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
	"time"

	"github.com/gorilla/mux"
	"github.com/sirupsen/logrus"

	"control-plane-agent/internal/model"
	"control-plane-agent/internal/registry"
	"control-plane-agent/internal/telemetry"
)

func PopulateConnMetrics(status *model.ConnectionStatus, id string) {
	if status != nil {
		metric, ok := telemetry.Storage.GetMetric(id)
		// logrus.Debugf("DEBUG %v, %v, %v, %v", id, metric, ok, telemetry.Storage.Metrics)
		if ok {
			status.UpdatedAt = model.CustomTime(time.UnixMilli(metric.TimestampMs))
			if v, ok := metric.Fields["state"].(string); ok {
				status.State = v
			}
			if v, ok := metric.Fields["link"].(bool); ok {
				status.Linked = v
			}
			if v, ok := metric.Fields["in"].(uint64); ok {
				status.InboundBytes = v
			}
			if v, ok := metric.Fields["out"].(uint64); ok {
				status.OutboundBytes = v
			}
			if v, ok := metric.Fields["strn"].(uint64); ok {
				status.TransactionsSucceeded = uint32(v)
			}
			if v, ok := metric.Fields["ftrn"].(uint64); ok {
				status.TransactionsFailed = uint32(v)
			}
			if v, ok := metric.Fields["tps"].(float64); ok {
				status.TransactionsPerSecond = v
			}
			if v, ok := metric.Fields["inbw"].(float64); ok {
				status.InboundBandwidth = v
			}
			if v, ok := metric.Fields["outbw"].(float64); ok {
				status.OutboundBandwidth = v
			}
			if v, ok := metric.Fields["err"].(uint64); ok {
				status.Errors = uint32(v)
			}
			if v, ok := metric.Fields["errd"].(uint64); ok {
				status.ErrorsDelta = uint32(v)
			}
		}
	}
}

func (a *API) ListMediaProxies(w http.ResponseWriter, r *http.Request) {
	query := r.URL.Query()
	addStatus := query.Has("status")
	addConfig := query.Has("config")

	proxies, err := registry.MediaProxyRegistry.List(r.Context(), nil, addStatus, addConfig)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}

	for i := range proxies {
		if len(proxies[i].ConnIds) > 0 {
			conns, err := registry.ConnRegistry.List(r.Context(), proxies[i].ConnIds, true, true)
			if err == nil {
				for j := range conns {
					conns[j].ProxyId = "" // Hide the id in JSON (omitempty)
					PopulateConnMetrics(conns[j].Status, conns[j].Id)
				}
				proxies[i].Conns = conns
			}
		}
		if len(proxies[i].BridgeIds) > 0 {
			bridges, err := registry.BridgeRegistry.List(r.Context(), proxies[i].BridgeIds, true, true)
			if err == nil {
				for j := range bridges {
					bridges[j].ProxyId = "" // Hide the id in JSON (omitempty)
					PopulateConnMetrics(bridges[j].Status, bridges[j].Id)
				}
				proxies[i].Bridges = bridges
			}
		}
	}

	resp := struct {
		MediaProxy []model.MediaProxy `json:"mediaProxy"`
	}{
		MediaProxy: proxies,
	}
	applyContentTypeHeaderJSON(w)
	err = json.NewEncoder(w).Encode(resp)
	if err != nil {
		logrus.Debugf("PROXIES JSON err %v", err)
	}
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

	item.Id = "" // Hide the id in JSON (omitempty)

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
