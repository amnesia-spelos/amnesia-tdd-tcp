# Returns the body of the function declared by $Signature in $Source, from its opening brace to
# the matching closing brace. Every brace counts, including ones inside string or char literals
# and comments; that is acceptable for the functions the source checks target.
function Get-FunctionBody {
    param(
        [Parameter(Mandatory, Position = 0)]
        [string]$Source,

        [Parameter(Mandatory, Position = 1)]
        [string]$Signature
    )

    $start = $Source.IndexOf($Signature)
    if ($start -lt 0) { throw "Could not find $Signature." }
    $open = $Source.IndexOf('{', $start + $Signature.Length)
    if ($open -lt 0) { throw "Could not isolate $Signature." }
    $depth = 0
    for ($i = $open; $i -lt $Source.Length; $i++) {
        if ($Source[$i] -eq '{') { $depth++ }
        elseif ($Source[$i] -eq '}') {
            $depth--
            if ($depth -eq 0) { return $Source.Substring($open, $i - $open + 1) }
        }
    }
    throw "Could not isolate $Signature."
}
