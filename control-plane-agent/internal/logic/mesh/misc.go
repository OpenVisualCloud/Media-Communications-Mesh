package mesh

import (
	"fmt"
	"strconv"
	"strings"
)

func ParseGroupURN(groupUrn string) (ip string, port uint16, err error) {
	parts := strings.Split(groupUrn, ":")
	if len(parts) != 2 {
		err = fmt.Errorf("invalid group urn format: %s", groupUrn)
		return
	}

	ip = parts[0]

	portStr := parts[1]
	portInt, err := strconv.Atoi(portStr)
	if err != nil {
		err = fmt.Errorf("invalid port number in group urn '%s': %s", groupUrn, portStr)
		return
	}

	if portInt < 0 || portInt > 65535 {
		err = fmt.Errorf("port number out of range in group urn '%s': %s", groupUrn, portStr)
		return
	}

	port = uint16(portInt)
	return
}
