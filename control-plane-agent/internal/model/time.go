package model

import (
	"encoding/json"
	"time"
)

type CustomTime time.Time

// Only 3 digits after decimal point - for web UI time series
func (ct CustomTime) MarshalJSON() ([]byte, error) {
	t := time.Time(ct)
	if t.IsZero() {
		return []byte("null"), nil
	}
	return json.Marshal(t.Format("2006-01-02T15:04:05.000Z07:00"))
}
