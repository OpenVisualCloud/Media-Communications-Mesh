package utils

import (
	"testing"

	"github.com/stretchr/testify/require"
)

func TestMergeStringSlices(t *testing.T) {
	tests := []struct {
		sliceA   []string
		sliceB   []string
		expected []string
	}{
		{
			sliceA:   []string{"a", "b", "c", "d"},
			sliceB:   []string{"c", "a", "e", "b", "f"},
			expected: []string{"a", "b", "c", "e", "f"},
		},
		{
			sliceA:   []string{"x", "y", "z", "a", "b"},
			sliceB:   []string{"a", "n", "x", "y"},
			expected: []string{"x", "y", "a", "n"},
		},
	}

	for _, tt := range tests {
		result := ReconcileOrderedStringSlices(tt.sliceA, tt.sliceB)
		require.Equal(t, tt.expected, result)
	}
}
