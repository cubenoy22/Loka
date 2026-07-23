param(
    [int]$Port = 23946
)

$ErrorActionPreference = "Stop"
$client = [Net.Sockets.TcpClient]::new()

try {
    $client.Connect("localhost", $Port)
    $networkStream = $client.GetStream()
    $gdbInput = [Console]::OpenStandardInput()
    $gdbOutput = [Console]::OpenStandardOutput()

    # Keep the GDB remote protocol binary-clean in both directions.
    $toMame = $gdbInput.CopyToAsync($networkStream)
    $toGdb = $networkStream.CopyToAsync($gdbOutput)
    $completed = [Threading.Tasks.Task]::WaitAny(
        [Threading.Tasks.Task[]]@($toMame, $toGdb))
    if ($completed -eq 0) {
        # GDB closed its input first. Preserve MAME's final response before exit.
        [void]$toMame.GetAwaiter().GetResult()
        $client.Client.Shutdown([Net.Sockets.SocketShutdown]::Send)
        [void]$toGdb.GetAwaiter().GetResult()
    } else {
        [void]$toGdb.GetAwaiter().GetResult()
    }
} finally {
    $client.Dispose()
}
