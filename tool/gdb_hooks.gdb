# GDB hooks for remote OpenOCD cleanup
# These hooks automatically shutdown OpenOCD when debug session ends

# Hook after pio_reset_run_target (PlatformIO's reset and run command)
define hookpost-pio_reset_run_target
    echo [gdb_hooks] pio_reset_run_target completed, shutting down OpenOCD\n
    monitor shutdown
    quit
end

# Hook after detach command (when debugger detaches from target)
define hookpost-detach
    echo [gdb_hooks] detach completed, shutting down OpenOCD\n
    monitor shutdown
    quit
end

# Hook after quit command (explicit GDB quit)
define hookpre-quit
    echo [gdb_hooks] quit requested, shutting down OpenOCD\n
    monitor shutdown
end

# Also hook the continue command for safety
define hookpost-continue
    # Only shutdown if we're detaching after continue
    # This might be called when PlatformIO does reset+run+detach sequence
end
