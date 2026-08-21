param(
    [Parameter(Mandatory = $true)]
    [string]$Path
)

$ErrorActionPreference = "Stop"

$ResolvedPath = (Resolve-Path -LiteralPath $Path).Path
$Stream = [System.IO.File]::OpenRead($ResolvedPath)

try {
    $Reader = [System.IO.BinaryReader]::new($Stream)

    if ($Reader.ReadUInt16() -ne 0x5A4D) {
        throw "Not a valid PE file: missing MZ header."
    }

    $Stream.Position = 0x3C
    $PeOffset = $Reader.ReadUInt32()
    $Stream.Position = $PeOffset

    if ($Reader.ReadUInt32() -ne 0x00004550) {
        throw "Not a valid PE file: missing PE signature."
    }

    $Machine = $Reader.ReadUInt16()
}
finally {
    $Stream.Dispose()
}

if ($Machine -ne 0x014C) {
    throw ("Wrong architecture: expected x86 (0x014C), actual 0x{0:X4}." -f $Machine)
}

$Item = Get-Item -LiteralPath $ResolvedPath
$Hash = (Get-FileHash -LiteralPath $ResolvedPath -Algorithm SHA256).Hash.ToLowerInvariant()

[pscustomobject]@{
    Path = $ResolvedPath
    Architecture = "x86 / 32-bit"
    Machine = "0x014C"
    Size = $Item.Length
    SHA256 = $Hash
} | Format-List
