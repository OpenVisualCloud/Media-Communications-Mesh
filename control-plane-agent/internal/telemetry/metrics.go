package telemetry

import (
	"sync"
)

type Metric struct {
	TimestampMs int64
	Fields      map[string]interface{}
}

func NewMetric(timestampMs int64) Metric {
	return Metric{TimestampMs: timestampMs, Fields: make(map[string]interface{})}
}

type storage struct {
	Metrics map[string]Metric
	mx      sync.RWMutex
}

var Storage storage = storage{
	Metrics: make(map[string]Metric),
}

// TODO: Add a Delete function to delete an id when it is removed from registry.

func (s *storage) AddMetric(id string, metric Metric) {
	s.mx.Lock()
	defer s.mx.Unlock()
	s.Metrics[id] = metric
}

func (s *storage) GetMetric(id string) (Metric, bool) {
	s.mx.RLock()
	defer s.mx.RUnlock()
	metric, exists := s.Metrics[id]
	return metric, exists
}
