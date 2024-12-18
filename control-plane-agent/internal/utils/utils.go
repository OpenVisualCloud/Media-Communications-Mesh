package utils

// ReconcileOrderedStringSlices takes two string slices, sliceA and sliceB, and returns
// a resulting slice that contains all items of sliceA, maintaining the order
// of common items and appending new items at the end.
func ReconcileOrderedStringSlices(sliceA, sliceB []string) []string {
	lenA := len(sliceA)
	lenB := len(sliceB)

	if lenA == 0 {
		return sliceB
	} else if lenB == 0 {
		return sliceA
	}

	mapSliceB := make(map[string]bool, lenB)
	for _, v := range sliceB {
		mapSliceB[v] = true
	}

	result := make([]string, 0, lenB)

	for _, v := range sliceA {
		if mapSliceB[v] {
			result = append(result, v)
			mapSliceB[v] = false
		}
	}

	for _, v := range sliceB {
		if mapSliceB[v] {
			result = append(result, v)
		}
	}
	return result
}

// Intersection returns a slice of strings that exist in both first and second slices.
func Intersection(first, second []string) []string {
	m := map[string]struct{}{}
	for _, str := range first {
		m[str] = struct{}{}
	}

	var res []string
	for _, str := range second {
		if _, ok := m[str]; ok {
			res = append(res, str)
		}
	}
	return res
}
