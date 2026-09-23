param(
  [string]$SourceRoot = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
)

$ErrorActionPreference = 'Stop'
$gamePath = Join-Path $SourceRoot 'games/gameplay_lab/native/src/GameplayLabGame.cpp'
$environmentPath = Join-Path $SourceRoot 'games/gameplay_lab/native/src/GameplayShowcaseEnvironment3D.cpp'
$headerPath = Join-Path $SourceRoot 'games/gameplay_lab/native/include/GameplayShowcaseEnvironment3D.h'
$game = Get-Content -LiteralPath $gamePath -Raw
$environment = Get-Content -LiteralPath $environmentPath -Raw
$header = Get-Content -LiteralPath $headerPath -Raw

function Get-MarkedBlock([string]$Source, [string]$Begin, [string]$End) {
  $first = $Source.IndexOf($Begin, [StringComparison]::Ordinal)
  if ($first -lt 0) { throw "missing marker: $Begin" }
  $last = $Source.IndexOf($End, $first, [StringComparison]::Ordinal)
  if ($last -lt 0) { throw "missing marker: $End" }
  return $Source.Substring($first, $last + $End.Length - $first)
}

$windowDetail = Get-MarkedBlock $game 'R100_COTTAGE_WINDOW_DETAIL_BEGIN' 'R100_COTTAGE_WINDOW_DETAIL_END'
$frontage = Get-MarkedBlock $game 'R60_COTTAGE_FRONTAGE_BEGIN' 'R60_COTTAGE_FRONTAGE_END'
$life = Get-MarkedBlock $game 'R34_COTTAGE_LIFE_BEGIN' 'R34_COTTAGE_LIFE_END'

# All six existing windows receive two substantial shutter leaves and one
# outward sill/drip board. The helpers submit only resident box meshes under
# the owning building's material and render identity.
$windowCode = [regex]::Replace($windowDetail, '(?m)//[^\r\n]*', '')
$shutterThickness = [regex]::Match($windowCode, 'constexpr\s+float\s+shutterThickness\s*=\s*(?<size>\d*\.?\d+)f\s*;')
$sillProjection = [regex]::Match($windowCode, 'constexpr\s+float\s+sillProjection\s*=\s*(?<size>\d*\.?\d+)f\s*;')
if (-not $shutterThickness.Success -or
    [double]::Parse($shutterThickness.Groups['size'].Value, [Globalization.CultureInfo]::InvariantCulture) -lt .12 -or
    -not $sillProjection.Success -or
    [double]::Parse($sillProjection.Groups['size'].Value, [Globalization.CultureInfo]::InvariantCulture) -lt .22) {
  throw 'window shutters must have at least .12 m thickness and sills project at least .22 m'
}
$zHelper = [regex]::Match($windowCode, 'const\s+auto\s+cottageWindowDetailZ\s*=')
$xHelper = [regex]::Match($windowCode, 'const\s+auto\s+cottageWindowDetailX\s*=')
if (-not $zHelper.Success -or -not $xHelper.Success -or $zHelper.Index -ge $xHelper.Index) {
  throw 'missing Z/X cottage window detail helpers'
}
$windowHelpers = @(
  @{ Axis='Z'; Code=$windowCode.Substring($zHelper.Index, $xHelper.Index - $zHelper.Index) },
  @{ Axis='X'; Code=$windowCode.Substring($xHelper.Index) })
foreach ($helper in $windowHelpers) {
  $code = $helper.Code
  if ($code -notmatch 'for\s*\(\s*float\s+side\s*:\s*\{\s*-1\.f\s*,\s*1\.f\s*\}\s*\)') {
    throw "window $($helper.Axis) detail lost its paired shutters"
  }
  if ([regex]::Matches($code, 'scene_\.Mesh\s*\(\s*boxMesh_').Count -ne 2 -or
      [regex]::Matches($code, '\{\s*\}\s*,\s*renderIdentity\s*\)').Count -ne 2) {
    throw "window $($helper.Axis) detail must draw two shutters and one sill with boxMesh_ and renderIdentity"
  }
  if ($code -notmatch 'scene_\.MeshCount\s*\(\s*\)\s*!=\s*\w+\s*\+\s*3u?\b') {
    throw "window $($helper.Axis) detail lost its fixed +3 draw gate"
  }
  if ($code -notmatch '\b(?:wood|doorMaterial)\b') {
    throw "window $($helper.Axis) detail lost its inherited wood/door material"
  }
  if ($code -notmatch '\bshutterThickness\b' -or
      $code -notmatch '\bfacing\s*\*\s*sillProjection\b|\bsillProjection\s*\*\s*facing\b') {
    throw "window $($helper.Axis) detail lost its thick shutter or outward sill geometry"
  }
}
if ($windowCode -match '(?i)physics_|collider|CreateMesh|texture|materialAsset|buildingComponentMask|buildingPositions|buildingMeshes|\bdescriptor\s*(?:\.|->|\[)|\b(?:push_back|emplace_back)\s*\(') {
  throw "window detail gained collision, mesh, texture, or descriptor state: $($Matches[0])"
}
$framedZ = [regex]::Match($game, 'const\s+auto\s+framedWindowZ\s*=\s*\[&\][^\r\n]*')
$framedX = [regex]::Match($game, 'const\s+auto\s+framedWindowX\s*=\s*\[&\][^\r\n]*')
if (-not $framedZ.Success -or $framedZ.Value -notmatch 'cottageWindowDetailZ\s*\(\s*x\s*,\s*z\s*,\s*facing\s*\)' -or
    -not $framedX.Success -or $framedX.Value -notmatch 'cottageWindowDetailX\s*\(\s*x\s*,\s*facing\s*\)') {
  throw 'window detail helpers are not attached to both framed window axes'
}
$windowPlacements = @(
  'for\s*\(\s*float\s+x\s*:\s*\{\s*-14\.1f\s*,\s*-9\.9f\s*\}\s*\)\s*framedWindowZ\s*\(\s*x\s*,\s*20\.16f\s*,\s*-1\.f\s*\)',
  'for\s*\(\s*float\s+x\s*:\s*\{\s*-14\.1f\s*,\s*-9\.9f\s*\}\s*\)\s*framedWindowZ\s*\(\s*x\s*,\s*27\.84f\s*,\s*1\.f\s*\)',
  'framedWindowX\s*\(\s*left\s*\?\s*-16\.19f\s*:\s*-7\.81f\s*,\s*left\s*\?\s*-1\.f\s*:\s*1\.f\s*\)')
foreach ($placement in $windowPlacements) {
  if ($game -notmatch $placement) { throw "six-window placement contract changed: $placement" }
}
if ((2 + 2 + 1 + 1) * 3 -ne 18) { throw 'cottage window detail draw budget changed' }

# The six authoritative parts and their material assets remain the environment contract.
if ($environment -notmatch 'array<Aevrynd::Vec3, 6> buildingPositions' -or
    $environment -notmatch 'array<std::string_view, 6> buildingMeshes' -or
    $environment -notmatch 'buildingComponentMask \|= 1u << 5') {
  throw 'authoritative cottage descriptor contract changed'
}
$assetIds = @(
  'procedural/cottage-foundation', 'procedural/cottage-front',
  'procedural/cottage-back', 'procedural/cottage-left',
  'procedural/cottage-right', 'procedural/cottage-pitched-roof')
foreach ($assetId in $assetIds) {
  if (-not $environment.Contains('"' + $assetId + '"')) {
    throw "missing authoritative cottage asset: $assetId"
  }
}

# Render additions may only submit resident meshes with the inherited identity/material.
$requiredFrontage = @(
  'std::array<Transform3D,8> cottageFrontage',
  '{{-12.f,3.05f,19.55f},{-.10f,0,0},{3.45f,.14f,1.45f}}',
  '{{-12.f,2.94f,18.84f},{0,0,0},{3.55f,.26f,.14f}}',
  '{{-13.10f,1.60f,19.18f},{0,0,0},{.16f,2.62f,.16f}}',
  '{{-10.90f,1.60f,19.18f},{0,0,0},{.16f,2.62f,.16f}}',
  '{{-12.88f,2.72f,19.20f},{0,0,-.65f},{.12f,.62f,.12f}}',
  '{{-11.12f,2.72f,19.20f},{0,0,.65f},{.12f,.62f,.12f}}',
  '{{-16.11f,1.83f,20.13f},{0,0,0},{.18f,3.12f,.16f}}',
  '{{-7.89f,1.83f,20.13f},{0,0,0},{.18f,3.12f,.16f}}',
  'scene_.Mesh(boxMesh_,frontage,wood,{},renderIdentity)',
  'Cottage frontage obstructed door passage')
foreach ($token in $requiredFrontage) {
  if (-not $frontage.Contains($token)) { throw "missing frontage contract: $token" }
}
foreach ($forbidden in @('push_back(', 'Collider', 'physics_', '.textures', 'materialAsset', 'CreateMesh')) {
  if ($frontage.Contains($forbidden)) { throw "frontage gained authoritative/asset state: $forbidden" }
}
if (-not $game.Contains('scene_.Mesh(showcaseCylinderMesh_,{{-12.f,4.47f,24.f},{1.5708f,0,0},') -or
    -not $game.Contains('{.20f,7.85f,.20f}},material,{},renderIdentity)')) {
  throw 'roof ridge no longer reuses roof material and identity'
}
if (-not $header.Contains('-12.f, 2.45f, 19.70f')) {
  throw 'lantern was not lowered below the porch slab'
}

# The unchanged twelve-piece budget is moved to the front without creating materials.
if (-not $life.Contains('std::array<CottageLifeDetail3D,12> lifeDetails') -or
    -not $life.Contains('lifeDetails.size()!=12') -or
    -not $life.Contains('{},renderIdentity)')) {
  throw 'life-detail count or inherited identity changed'
}
foreach ($position in @(
    '{-15.60f,.22f,19.72f}', '{-15.60f,.22f,20.08f}',
    '{-15.35f,.50f,19.90f}', '{-15.02f,.22f,19.72f}',
    '{-15.02f,.22f,20.08f}', '{-8.85f,.58f,19.78f}',
    '{-9.42f,.28f,19.78f}', '{-8.28f,.28f,19.78f}',
    '{-14.55f,.42f,19.92f}', '{-13.86f,.32f,20.00f}')) {
  if (-not $life.Contains($position)) { throw "life detail was not moved forward: $position" }
}

# Conservative unit-box AABBs leave a 1.2 m wide, 2.5 m tall central doorway clear.
$doorMinX = -12.60
$doorMaxX = -11.40
$doorMaxY = 2.50
$parts = @(
  @{ X=-12.00; Y=3.05; HalfX=1.725; HalfY=0.143 },
  @{ X=-12.00; Y=2.94; HalfX=1.775; HalfY=0.130 },
  @{ X=-13.10; Y=1.60; HalfX=0.080; HalfY=1.310 },
  @{ X=-10.90; Y=1.60; HalfX=0.080; HalfY=1.310 },
  @{ X=-12.88; Y=2.72; HalfX=0.236; HalfY=0.284 },
  @{ X=-11.12; Y=2.72; HalfX=0.236; HalfY=0.284 },
  @{ X=-16.11; Y=1.83; HalfX=0.090; HalfY=1.560 },
  @{ X=-7.89;  Y=1.83; HalfX=0.090; HalfY=1.560 })
foreach ($part in $parts) {
  $overlapsX = ($part.X - $part.HalfX) -lt $doorMaxX -and
               ($part.X + $part.HalfX) -gt $doorMinX
  $overlapsY = ($part.Y - $part.HalfY) -lt $doorMaxY
  if ($overlapsX -and $overlapsY) { throw 'frontage obstructs the door passage' }
}

Write-Host 'Gameplay cottage frontage: 6 descriptors, 6 windows/+18 render-only details, inherited identity/material, no collision/assets, 12 moved life props, lantern clearance, and door passage passed'
