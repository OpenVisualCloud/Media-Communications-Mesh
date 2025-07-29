package model

import (
	"errors"
	"fmt"
	"strconv"
	"strings"

	"control-plane-agent/api/proxy/proto/sdk"
)

type SDKBufferPartition struct {
	Offset uint32 `json:"offset"`
	Size   uint32 `json:"size"`
}

type SDKBufferPartitions struct {
	Payload  SDKBufferPartition `json:"payload"`
	Metadata SDKBufferPartition `json:"metadata"`
	Sysdata  SDKBufferPartition `json:"sysdata"`
}

type SDKConfigMultipointGroup struct {
	URN string `json:"urn"`
}

type SDKConfigST2110 struct {
	IPAddr       string              `json:"remoteIpAddr"`
	Port         uint16              `json:"port"`
	McastSipAddr string              `json:"mcastSipAddr,omitempty"`
	Transport    sdk.ST2110Transport `json:"-"`
	TransportStr string              `json:"transport"`
	Pacing       string              `json:"pacing"`
	PayloadType  uint8               `json:"payloadType"`
}

type SDKConfigRDMA struct {
	ConnectionMode string `json:"connectionMode"`
	MaxLatencyNs   uint32 `json:"maxLatencyNs"`
}

type SDKConnectionOptionsRDMA struct {
	Provider     string `json:"provider,omitempty"`
	NumEndpoints uint8  `json:"numEndpoints,omitempty"`
}

type SDKConfigVideo struct {
	Width          uint32               `json:"width"`
	Height         uint32               `json:"height"`
	FPS            float64              `json:"fps"`
	PixelFormat    sdk.VideoPixelFormat `json:"-"`
	PixelFormatStr string               `json:"pixelFormat"`
}

type SDKConfigAudio struct {
	Channels      uint32              `json:"channels"`
	SampleRate    sdk.AudioSampleRate `json:"-"`
	SampleRateStr string              `json:"sampleRate"`
	Format        sdk.AudioFormat     `json:"-"`
	FormatStr     string              `json:"format"`
	PacketTime    sdk.AudioPacketTime `json:"-"`
	PacketTimeStr string              `json:"packetTime"`
}

type SDKConfigBlob struct {
}

type SDKConnectionConfig struct {
	BufQueueCapacity      uint32 `json:"bufQueueCapacity"`
	MaxPayloadSize        uint32 `json:"maxPayloadSize"`
	MaxMetadataSize       uint32 `json:"maxMetadataSize"`
	CalculatedPayloadSize uint32 `json:"calculatedPayloadSize"`

	BufParts SDKBufferPartitions `json:"bufPartitions"`

	Conn struct {
		MultipointGroup *SDKConfigMultipointGroup `json:"multipointGroup,omitempty"`
		ST2110          *SDKConfigST2110          `json:"st2110,omitempty"`
		RDMA            *SDKConfigRDMA            `json:"rdma,omitempty"`
	} `json:"conn"`

	Options struct {
		RDMA SDKConnectionOptionsRDMA `json:"rdma"`
	} `json:"options"`

	Payload struct {
		Video *SDKConfigVideo `json:"video,omitempty"`
		Audio *SDKConfigAudio `json:"audio,omitempty"`
		Blob  *SDKConfigBlob  `json:"blob,omitempty"`
	} `json:"payload"`
}

func (s *SDKConfigST2110) UpdateStringValues() {
	str, ok := sdk.ST2110Transport_name[int32(s.Transport)]
	if !ok {
		str = strconv.Itoa(int(s.Transport))
	}
	s.TransportStr = strings.Replace(strings.ToLower(strings.TrimPrefix(str, "CONN_TRANSPORT_")), "_", "-", 1)
}

func (s *SDKConfigVideo) UpdateStringValues() {
	str, ok := sdk.VideoPixelFormat_name[int32(s.PixelFormat)]
	if !ok {
		str = strconv.Itoa(int(s.PixelFormat))
	}
	s.PixelFormatStr = strings.ToLower(strings.TrimPrefix(str, "VIDEO_PIXEL_FORMAT_"))
}

func (s *SDKConfigAudio) UpdateStringValues() {
	str, ok := sdk.AudioSampleRate_name[int32(s.SampleRate)]
	if !ok {
		str = strconv.Itoa(int(s.SampleRate))
	}
	s.SampleRateStr = strings.ToLower(strings.TrimPrefix(str, "AUDIO_SAMPLE_RATE_"))

	str, ok = sdk.AudioFormat_name[int32(s.Format)]
	if !ok {
		str = strconv.Itoa(int(s.Format))
	}
	s.FormatStr = strings.Replace(strings.ToLower(strings.TrimPrefix(str, "AUDIO_FORMAT_")), "_", "-", 1)

	str, ok = sdk.AudioPacketTime_name[int32(s.PacketTime)]
	if !ok {
		str = strconv.Itoa(int(s.PacketTime))
	}
	s.PacketTimeStr = strings.Replace(strings.ToLower(strings.TrimPrefix(str, "AUDIO_PACKET_TIME_")), "_", ".", 1)
}

func (s *SDKConnectionConfig) UpdateStringValues() {
	if s.Conn.ST2110 != nil {
		s.Conn.ST2110.UpdateStringValues()
	}
	if s.Payload.Video != nil {
		s.Payload.Video.UpdateStringValues()
	}
	if s.Payload.Audio != nil {
		s.Payload.Audio.UpdateStringValues()
	}
}

func (s *SDKConnectionConfig) AssignFromPb(cfg *sdk.ConnectionConfig) error {
	if cfg == nil {
		return errors.New("sdk conn cfg is nil")
	}

	s.BufQueueCapacity = cfg.BufQueueCapacity
	s.MaxPayloadSize = cfg.MaxPayloadSize
	s.MaxMetadataSize = cfg.MaxMetadataSize
	s.CalculatedPayloadSize = cfg.CalculatedPayloadSize

	if cfg.BufParts == nil ||
		cfg.BufParts.Payload == nil ||
		cfg.BufParts.Metadata == nil ||
		cfg.BufParts.Sysdata == nil {
		return errors.New("sdk buf parts cfg is nil")
	}

	s.BufParts.Payload.Offset = cfg.BufParts.Payload.Offset
	s.BufParts.Payload.Size = cfg.BufParts.Payload.Size

	s.BufParts.Metadata.Offset = cfg.BufParts.Metadata.Offset
	s.BufParts.Metadata.Size = cfg.BufParts.Metadata.Size

	s.BufParts.Sysdata.Offset = cfg.BufParts.Sysdata.Offset
	s.BufParts.Sysdata.Size = cfg.BufParts.Sysdata.Size

	switch conn := cfg.Conn.(type) {
	case *sdk.ConnectionConfig_MultipointGroup:
		s.Conn.MultipointGroup = &SDKConfigMultipointGroup{
			URN: conn.MultipointGroup.Urn,
		}
	case *sdk.ConnectionConfig_St2110:
		s.Conn.ST2110 = &SDKConfigST2110{
			IPAddr:       conn.St2110.IpAddr,
			Port:         uint16(conn.St2110.Port),
			McastSipAddr: conn.St2110.McastSipAddr,
			Transport:    conn.St2110.Transport,
			Pacing:       conn.St2110.Pacing,
			PayloadType:  uint8(conn.St2110.PayloadType),
		}
	case *sdk.ConnectionConfig_Rdma:
		s.Conn.RDMA = &SDKConfigRDMA{
			ConnectionMode: conn.Rdma.ConnectionMode,
			MaxLatencyNs:   conn.Rdma.MaxLatencyNs,
		}
	default:
		return errors.New("unknown sdk conn cfg type")
	}

	if cfg.Options != nil && cfg.Options.Rdma != nil {
		s.Options.RDMA.Provider = cfg.Options.Rdma.Provider
		s.Options.RDMA.NumEndpoints = uint8(cfg.Options.Rdma.NumEndpoints)
	}

	switch payload := cfg.Payload.(type) {
	case *sdk.ConnectionConfig_Video:
		s.Payload.Video = &SDKConfigVideo{
			Width:       payload.Video.Width,
			Height:      payload.Video.Height,
			FPS:         payload.Video.Fps,
			PixelFormat: payload.Video.PixelFormat,
		}
	case *sdk.ConnectionConfig_Audio:
		s.Payload.Audio = &SDKConfigAudio{
			Channels:   payload.Audio.Channels,
			SampleRate: payload.Audio.SampleRate,
			Format:     payload.Audio.Format,
			PacketTime: payload.Audio.PacketTime,
		}
	case *sdk.ConnectionConfig_Blob:
		s.Payload.Blob = &SDKConfigBlob{}
	default:
		return errors.New("unknown sdk conn cfg payload type")
	}

	s.UpdateStringValues()
	return nil
}

func (s *SDKConnectionConfig) AssignToPb(cfg *sdk.ConnectionConfig) {
	cfg.BufQueueCapacity = s.BufQueueCapacity
	cfg.MaxPayloadSize = s.MaxPayloadSize
	cfg.MaxMetadataSize = s.MaxMetadataSize
	cfg.CalculatedPayloadSize = s.CalculatedPayloadSize

	cfg.BufParts = &sdk.BufferPartitions{
		Payload: &sdk.BufferPartition{
			Offset: s.BufParts.Payload.Offset,
			Size:   s.BufParts.Payload.Size,
		},
		Metadata: &sdk.BufferPartition{
			Offset: s.BufParts.Metadata.Offset,
			Size:   s.BufParts.Metadata.Size,
		},
		Sysdata: &sdk.BufferPartition{
			Offset: s.BufParts.Sysdata.Offset,
			Size:   s.BufParts.Sysdata.Size,
		},
	}

	switch {
	case s.Conn.MultipointGroup != nil:
		cfg.Conn = &sdk.ConnectionConfig_MultipointGroup{
			MultipointGroup: &sdk.ConfigMultipointGroup{
				Urn: s.Conn.MultipointGroup.URN,
			},
		}
	case s.Conn.ST2110 != nil:
		cfg.Conn = &sdk.ConnectionConfig_St2110{
			St2110: &sdk.ConfigST2110{
				IpAddr:       s.Conn.ST2110.IPAddr,
				Port:         uint32(s.Conn.ST2110.Port),
				McastSipAddr: s.Conn.ST2110.McastSipAddr,
				Transport:    s.Conn.ST2110.Transport,
				Pacing:       s.Conn.ST2110.Pacing,
				PayloadType:  uint32(s.Conn.ST2110.PayloadType),
			},
		}
	case s.Conn.RDMA != nil:
		cfg.Conn = &sdk.ConnectionConfig_Rdma{
			Rdma: &sdk.ConfigRDMA{
				ConnectionMode: s.Conn.RDMA.ConnectionMode,
				MaxLatencyNs:   s.Conn.RDMA.MaxLatencyNs,
			},
		}
	}

	cfg.Options = &sdk.ConnectionOptions{
		Rdma: &sdk.ConnectionOptionsRDMA{
			Provider:     s.Options.RDMA.Provider,
			NumEndpoints: uint32(s.Options.RDMA.NumEndpoints),
		},
	}

	switch {
	case s.Payload.Video != nil:
		cfg.Payload = &sdk.ConnectionConfig_Video{
			Video: &sdk.ConfigVideo{
				Width:       s.Payload.Video.Width,
				Height:      s.Payload.Video.Height,
				Fps:         s.Payload.Video.FPS,
				PixelFormat: s.Payload.Video.PixelFormat,
			},
		}
	case s.Payload.Audio != nil:
		cfg.Payload = &sdk.ConnectionConfig_Audio{
			Audio: &sdk.ConfigAudio{
				Channels:   s.Payload.Audio.Channels,
				SampleRate: s.Payload.Audio.SampleRate,
				Format:     s.Payload.Audio.Format,
				PacketTime: s.Payload.Audio.PacketTime,
			},
		}
	case s.Payload.Blob != nil:
		cfg.Payload = &sdk.ConnectionConfig_Blob{
			Blob: &sdk.ConfigBlob{},
		}
	}
}

func (s *SDKConnectionConfig) CheckPayloadCompatibility(c *SDKConnectionConfig) error {
	if c == nil {
		return errors.New("sdk cfg is nil")
	}
	if s.CalculatedPayloadSize != c.CalculatedPayloadSize {
		return fmt.Errorf("incompatible calculated payload size: %v != %v", s.CalculatedPayloadSize, c.CalculatedPayloadSize)
	}

	switch {
	case s.Conn.MultipointGroup != nil:
		if c.Conn.MultipointGroup == nil {
			return errors.New("no multipoint group cfg")
		}
		if s.Conn.MultipointGroup.URN != c.Conn.MultipointGroup.URN {
			return fmt.Errorf("wrong multipoint group urn: '%v' vs. '%v'", s.Conn.MultipointGroup.URN, c.Conn.MultipointGroup.URN)
		}
	case s.Conn.ST2110 != nil:
		if c.Conn.ST2110 == nil {
			return errors.New("no st2110 cfg")
		}
		if s.Conn.ST2110.IPAddr != c.Conn.ST2110.IPAddr ||
			s.Conn.ST2110.Port != c.Conn.ST2110.Port {
			return fmt.Errorf("wrong st2110 remote host: %v:%v vs. %v:%v",
				s.Conn.ST2110.IPAddr, s.Conn.ST2110.Port,
				c.Conn.ST2110.IPAddr, c.Conn.ST2110.Port)
		}
		if s.Conn.ST2110.Transport != c.Conn.ST2110.Transport {
			return fmt.Errorf("incompatible st2110 transport: %v vs. %v", s.Conn.ST2110.Transport, c.Conn.ST2110.Transport)
		}
		if s.Conn.ST2110.PayloadType != c.Conn.ST2110.PayloadType {
			return fmt.Errorf("incompatible st2110 payload type: %v vs. %v", s.Conn.ST2110.PayloadType, c.Conn.ST2110.PayloadType)
		}
	}

	if s.Options.RDMA.Provider != c.Options.RDMA.Provider {
		return fmt.Errorf("incompatible rdma provider: %v vs. %v", s.Options.RDMA.Provider, c.Options.RDMA.Provider)
	}
	if s.Options.RDMA.NumEndpoints != c.Options.RDMA.NumEndpoints {
		return fmt.Errorf("incompatible rdma number of endpoints: %v vs. %v", s.Options.RDMA.NumEndpoints, c.Options.RDMA.NumEndpoints)
	}

	switch {
	case s.Payload.Video != nil:
		if c.Payload.Video == nil {
			return errors.New("no video cfg")
		}
		if s.Payload.Video.Width != c.Payload.Video.Width ||
			s.Payload.Video.Height != c.Payload.Video.Height ||
			s.Payload.Video.FPS != c.Payload.Video.FPS ||
			s.Payload.Video.PixelFormat != c.Payload.Video.PixelFormat {
			return fmt.Errorf("incompatible video: w:%v h:%v fps:%v fmt:%v vs. w:%v h:%v fps:%v fmt:%v",
				s.Payload.Video.Width, s.Payload.Video.Height, s.Payload.Video.FPS, s.Payload.Video.PixelFormat,
				c.Payload.Video.Width, c.Payload.Video.Height, c.Payload.Video.FPS, c.Payload.Video.PixelFormat)
		}
	case s.Payload.Audio != nil:
		if c.Payload.Audio == nil {
			return errors.New("no audio cfg")
		}
		if s.Payload.Audio.Channels != c.Payload.Audio.Channels ||
			s.Payload.Audio.SampleRate != c.Payload.Audio.SampleRate ||
			s.Payload.Audio.Format != c.Payload.Audio.Format ||
			s.Payload.Audio.PacketTime != c.Payload.Audio.PacketTime {
			return fmt.Errorf("incompatible audio: ch:%v sampling:%v fmt:%v ptime:%v vs. ch:%v sampling:%v fmt:%v ptime:%v",
				s.Payload.Audio.Channels, s.Payload.Audio.SampleRate, s.Payload.Audio.Format, s.Payload.Audio.PacketTime,
				c.Payload.Audio.Channels, c.Payload.Audio.SampleRate, c.Payload.Audio.Format, c.Payload.Audio.PacketTime)
		}
	case s.Payload.Blob != nil:
		if c.Payload.Blob == nil {
			return errors.New("no blob cfg")
		}
		if s.MaxPayloadSize != c.MaxPayloadSize {
			return fmt.Errorf("incompatible blob: sz:%v vs. sz:%v", s.MaxPayloadSize, c.MaxPayloadSize)
		}
	default:
		return errors.New("unknown payload type")
	}

	return nil
}

func (s *SDKConnectionConfig) CopyFrom(c *SDKConnectionConfig) {
	*s = *c

	switch {
	case c.Conn.MultipointGroup != nil:
		v := *c.Conn.MultipointGroup
		s.Conn.MultipointGroup = &v
	case c.Conn.ST2110 != nil:
		v := *c.Conn.ST2110
		s.Conn.ST2110 = &v
	case c.Conn.RDMA != nil:
		v := *c.Conn.RDMA
		s.Conn.RDMA = &v
	}

	switch {
	case c.Payload.Video != nil:
		v := *c.Payload.Video
		s.Payload.Video = &v
	case c.Payload.Audio != nil:
		v := *c.Payload.Audio
		s.Payload.Audio = &v
	}
}

func (s *SDKConnectionConfig) ConnType() string {
	switch {
	case s.Conn.MultipointGroup != nil:
		return "group"
	case s.Conn.ST2110 != nil:
		return "st2110"
	case s.Conn.RDMA != nil:
		return "rdma"
	default:
		return "?unknown?"
	}
}

func (s *SDKConnectionConfig) GetMultipointGroupURN() (string, error) {
	switch {
	case s.Conn.MultipointGroup != nil:
		return s.Conn.MultipointGroup.URN, nil
	case s.Conn.ST2110 != nil:
		return fmt.Sprintf("%v:%v", s.Conn.ST2110.IPAddr, s.Conn.ST2110.Port), nil
	default:
		return "", errors.New("can't get multipoint group urn: no cfg")
	}
}
