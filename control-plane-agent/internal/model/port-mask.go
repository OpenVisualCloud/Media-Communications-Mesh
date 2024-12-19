package model

import (
	"errors"
	"strconv"
	"strings"
)

var ErrNoAvailablePorts = errors.New("no available ports")
var ErrInvalidPortRange = errors.New("invalid port range")

type PortMask struct {
	bits       [1024]uint64
	initialBit uint32
}

func (pm *PortMask) SetBit(index uint16) {
	pm.bits[index/64] |= 1 << (index % 64)
	if uint32(index) == pm.initialBit {
		pm.initialBit++
	}
}

func (pm *PortMask) ClearBit(index uint16) {
	pm.bits[index/64] &^= 1 << (index % 64)
	if uint32(index) < pm.initialBit {
		pm.initialBit = uint32(index)
	}
}

func (pm *PortMask) IsBitSet(index uint16) bool {
	return pm.bits[index/64]&(1<<(index%64)) != 0
}

func (pm *PortMask) AllocateFirstAvailablePort(allowedMask *PortMask) (uint16, error) {
	for i := pm.initialBit; i <= 65535; i++ {
		if allowedMask.bits[i/64]&(1<<(i%64)) != 0 {
			if pm.bits[i/64]&(1<<(i%64)) == 0 {
				pm.bits[i/64] |= 1 << (i % 64)
				pm.initialBit = i + 1
				return uint16(i), nil
			}
		}
	}
	return 0, ErrNoAvailablePorts
}

func NewPortMask(portRanges string) (PortMask, error) {
	pm := PortMask{}
	ranges := strings.Split(portRanges, ",")
	firstBitSet := false
	for _, r := range ranges {
		r = strings.TrimSpace(r)
		if strings.Contains(r, "-") {
			parts := strings.Split(r, "-")
			if len(parts) != 2 {
				return PortMask{}, ErrInvalidPortRange
			}
			start, err := strconv.Atoi(strings.TrimSpace(parts[0]))
			if err != nil {
				return PortMask{}, ErrInvalidPortRange
			}
			end, err := strconv.Atoi(strings.TrimSpace(parts[1]))
			if err != nil {
				return PortMask{}, ErrInvalidPortRange
			}
			if start > end || start < 0 || end > 65535 {
				return PortMask{}, ErrInvalidPortRange
			}
			for i := start; i <= end; i++ {
				pm.bits[i/64] |= 1 << (i % 64)
			}
			if !firstBitSet || uint32(start) < pm.initialBit {
				pm.initialBit = uint32(start)
				firstBitSet = true
			}
		} else {
			port, err := strconv.Atoi(strings.TrimSpace(r))
			if err != nil {
				return PortMask{}, ErrInvalidPortRange
			}
			if port < 0 || port > 65535 {
				return PortMask{}, ErrInvalidPortRange
			}
			pm.bits[port/64] |= 1 << (port % 64)
			if !firstBitSet || uint32(port) < pm.initialBit {
				pm.initialBit = uint32(port)
				firstBitSet = true
			}
		}
	}
	return pm, nil
}
