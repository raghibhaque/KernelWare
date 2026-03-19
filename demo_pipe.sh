#!/bin/bash
# Pipe Dream FIFO demo — run this in a second terminal while playing Pipe Dream.
# Shows the kernel buffer filling and draining in real time via /proc/kernelware/stats.

if [ ! -r /proc/kernelware/stats ]; then
    echo "ERROR: /proc/kernelware/stats not found — is the driver loaded?"
    echo "Run: sudo insmod driver/kernelware.ko"
    exit 1
fi

echo "=== KernelWare Pipe Dream Live Demo ==="
echo "Start the game in another terminal and select Pipe Dream."
echo "Watching /proc/kernelware/stats (Ctrl+C to stop)..."
echo ""
watch -n 0.2 'cat /proc/kernelware/stats'
