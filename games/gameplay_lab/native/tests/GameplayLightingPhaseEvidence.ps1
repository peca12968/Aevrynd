$ErrorActionPreference = 'Stop'

function Assert-GameplayLightingPhaseLuminance {
    param(
        [Parameter(Mandatory = $true)][string]$CaptureDirectory,
        [Parameter(Mandatory = $true)][string]$Label,
        [ValidateRange(1.0, 2.0)][double]$MinimumRatio = 1.05
    )

    Add-Type -AssemblyName System.Drawing
    $luminance = @{}
    $results = @()

    foreach ($view in @('first', 'third', 'god')) {
        foreach ($phase in @('day', 'dusk', 'night')) {
            $name = "$view-$phase.png"
            $path = Join-Path $CaptureDirectory $name
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
                throw "Lighting phase capture missing: label=$Label capture=$name"
            }

            $bitmap = [System.Drawing.Bitmap]::FromFile($path)
            try {
                $sum = 0.0
                $samples = 0
                for ($y = 32; $y -lt $bitmap.Height; $y += 4) {
                    for ($x = 0; $x -lt $bitmap.Width; $x += 4) {
                        $color = $bitmap.GetPixel($x, $y)
                        $sum += 0.2126 * $color.R + 0.7152 * $color.G + 0.0722 * $color.B
                        ++$samples
                    }
                }
                if ($samples -lt 1) {
                    throw "Lighting phase capture has no samples: label=$Label capture=$name"
                }
                $luminance[$name] = $sum / $samples / 255.0
            }
            finally {
                $bitmap.Dispose()
            }
        }

        $day = [double]$luminance["$view-day.png"]
        $dusk = [double]$luminance["$view-dusk.png"]
        $night = [double]$luminance["$view-night.png"]
        $dayDuskRatio = if ($dusk -gt 0.0) { $day / $dusk } else { [double]::PositiveInfinity }
        $duskNightRatio = if ($night -gt 0.0) { $dusk / $night } else { [double]::PositiveInfinity }
        if ($day -lt $dusk * $MinimumRatio -or $dusk -lt $night * $MinimumRatio) {
            throw "Lighting phase luminance gate failed: label=$Label view=$view day=$day dusk=$dusk night=$night dayDuskRatio=$dayDuskRatio duskNightRatio=$duskNightRatio minimumRatio=$MinimumRatio"
        }
        $results += [pscustomobject]@{
            label = $Label
            view = $view
            day = $day
            dusk = $dusk
            night = $night
            dayDuskRatio = $dayDuskRatio
            duskNightRatio = $duskNightRatio
            minimumRatio = $MinimumRatio
        }
    }

    return $results
}
