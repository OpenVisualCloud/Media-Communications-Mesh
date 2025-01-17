package proxy

import (
	"context"
	"testing"

	"github.com/stretchr/testify/assert"

	pb "control-plane-agent/api/proxy/proto/mediaproxy"
	"control-plane-agent/internal/telemetry"
)

func TestProxyAPI_SendMetrics(t *testing.T) {
	cases := []struct {
		req      *pb.SendMetricsRequest
		expected map[string]telemetry.Metric
	}{
		{
			req: &pb.SendMetricsRequest{
				Metrics: []*pb.Metric{
					{
						TimestampMs: 1234567890,
						ProviderId:  "abc",
						Fields: []*pb.MetricField{
							{
								Name:  "state",
								Value: &pb.MetricField_StrValue{StrValue: "initial"},
							},
							{
								Name:  "bandwidth",
								Value: &pb.MetricField_UintValue{UintValue: 987654321},
							},
							{
								Name:  "fps",
								Value: &pb.MetricField_DoubleValue{DoubleValue: 59.94},
							},
							{
								Name:  "linked",
								Value: &pb.MetricField_BoolValue{BoolValue: true},
							},
						},
					},
				},
			},
			expected: map[string]telemetry.Metric{
				"abc": {
					TimestampMs: 1234567890,
					Fields: map[string]interface{}{
						"state":     "initial",
						"bandwidth": uint64(987654321),
						"fps":       59.94,
						"linked":    true,
					},
				},
			},
		},
		{
			req: &pb.SendMetricsRequest{
				Metrics: []*pb.Metric{
					{
						TimestampMs: 1234567890,
						ProviderId:  "abc",
						Fields: []*pb.MetricField{
							{
								Name:  "state",
								Value: &pb.MetricField_StrValue{StrValue: "initial"},
							},
							{
								Name:  "bandwidth",
								Value: &pb.MetricField_UintValue{UintValue: 987654321},
							},
							{
								Name:  "fps",
								Value: &pb.MetricField_DoubleValue{DoubleValue: 59.94},
							},
							{
								Name:  "linked",
								Value: &pb.MetricField_BoolValue{BoolValue: true},
							},
						},
					},
					{
						TimestampMs: 1234567891,
						ProviderId:  "abc",
						Fields: []*pb.MetricField{
							{
								Name:  "state",
								Value: &pb.MetricField_StrValue{StrValue: "closed"},
							},
							{
								Name:  "bandwidth",
								Value: &pb.MetricField_UintValue{UintValue: 3000000},
							},
							{
								Name:  "fps",
								Value: &pb.MetricField_DoubleValue{DoubleValue: 60.0},
							},
							{
								Name:  "linked",
								Value: &pb.MetricField_BoolValue{BoolValue: false},
							},
						},
					},
				},
			},
			expected: map[string]telemetry.Metric{
				"abc": {
					TimestampMs: 1234567891,
					Fields: map[string]interface{}{
						"state":     "closed",
						"bandwidth": uint64(3000000),
						"fps":       60.0,
						"linked":    false,
					},
				},
			},
		},
		{
			req: &pb.SendMetricsRequest{
				Metrics: []*pb.Metric{
					{
						TimestampMs: 1234567890,
						ProviderId:  "abc-1",
						Fields: []*pb.MetricField{
							{
								Name:  "state",
								Value: &pb.MetricField_StrValue{StrValue: "active"},
							},
							{
								Name:  "bandwidth",
								Value: &pb.MetricField_UintValue{UintValue: 1000000},
							},
							{
								Name:  "fps",
								Value: &pb.MetricField_DoubleValue{DoubleValue: 30.0},
							},
						},
					},
					{
						TimestampMs: 1234567891,
						ProviderId:  "abc-2",
						Fields: []*pb.MetricField{
							{
								Name:  "bandwidth",
								Value: &pb.MetricField_UintValue{UintValue: 1122334455},
							},
							{
								Name:  "fps",
								Value: &pb.MetricField_DoubleValue{DoubleValue: 60.0},
							},
							{
								Name:  "linked",
								Value: &pb.MetricField_BoolValue{BoolValue: false},
							},
						},
					},
				},
			},
			expected: map[string]telemetry.Metric{
				"abc-1": {
					TimestampMs: 1234567890,
					Fields: map[string]interface{}{
						"state":     "active",
						"bandwidth": uint64(1000000),
						"fps":       30.0,
					},
				},
				"abc-2": {
					TimestampMs: 1234567891,
					Fields: map[string]interface{}{
						"bandwidth": uint64(1122334455),
						"fps":       60.0,
						"linked":    false,
					},
				},
			},
		},
	}

	for _, v := range cases {
		api := API{}

		_, err := api.SendMetrics(context.Background(), v.req)
		assert.NoError(t, err)

		for providerId, expectedMetric := range v.expected {
			m, ok := telemetry.Storage.GetMetric(providerId)
			assert.True(t, ok)
			assert.Equal(t, expectedMetric.TimestampMs, m.TimestampMs)
			for fieldName, expectedValue := range expectedMetric.Fields {
				value, ok := m.Fields[fieldName]
				assert.True(t, ok)
				assert.Equal(t, expectedValue, value)
			}
		}
	}
}
