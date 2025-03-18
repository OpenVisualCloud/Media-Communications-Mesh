package utils

import "context"

type ReadinessChannel struct {
	ready      bool
	queue      chan bool
	notReadyCh chan struct{}
}

func NewReadinessChannel() *ReadinessChannel {
	return &ReadinessChannel{
		queue:      make(chan bool),
		notReadyCh: make(chan struct{}),
	}
}

func (rc *ReadinessChannel) Set(newState bool) {
	rc.queue <- newState
}

func (rc *ReadinessChannel) Run(ctx context.Context) {
	defer close(rc.notReadyCh)
	defer close(rc.queue)
	for {
		if rc.ready {
			select {
			case <-ctx.Done():
				return
			case newState := <-rc.queue:
				rc.ready = newState
			}
		} else {
			select {
			case <-ctx.Done():
				return
			case newState := <-rc.queue:
				rc.ready = newState
			case rc.notReadyCh <- struct{}{}:
			}
		}
	}
}

func (rc *ReadinessChannel) NotReady() <-chan struct{} {
	return rc.notReadyCh
}
