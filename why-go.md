# Why Go

Go optimizes for simplicity and fast development of concurrent, networked software.

## Simplicity
- A small, stable specification and one opinionated way to do most things.
- Readable code that is easy to review, onboard to, and maintain across large teams.

## Concurrency
- Goroutines make concurrent execution cheap and approachable.
- Channels and `select` provide structured communication between goroutines.
- The runtime schedules goroutines across cores without manual thread management.

## Deployment
- Compiles to a single static binary by default (disabled when using `cgo`); no runtime to install.
- Very fast compile times and one integrated toolchain (`go build`, `go test`, `gofmt`).
- Easy cross-compilation for servers and containers.

## Standard library & ecosystem
- A rich standard library covers HTTP, JSON, crypto, and more without third-party dependencies.
- A strong fit for networked and cloud services: Docker, Kubernetes, and Terraform are written in Go.

## Trade-offs / Limitations
- Garbage collection and the runtime make it a poor fit for hard real-time or latency-critical workloads.
- Less fine-grained control over memory layout and allocation than C, Rust, or Zig.

**Sources:** go.dev/solutions/ "Why Go"; go.dev/doc/ "The Go Programming Language"
