#requires -Version 7.0
# 地表與418部署資格獨立讀取；不表示目前幾何、原生49項或人工遊玩通過。
. (Join-Path $PSScriptRoot 'AuthoredNativeContract.ps1')

function Assert-MistwatchForestSurfaceScope($Value){
    $yes=@('runtime_identity_only','surface_recipe_CPU_passed')
    $no=@('current_geometry_qualified','native_qualified','native_executed','GPU_qualified','full49_qualified',
        'actual_Main_route_qualified','art_accepted','manual_play','published')
    Assert-MistwatchFallenExactKeys $Value ($yes+$no) 'surface-only exact scope'
    foreach($key in $yes){Assert-MistwatchAuthoredNativeBoolean $Value[$key] $true $key}
    foreach($key in $no){Assert-MistwatchAuthoredNativeBoolean $Value[$key] $false $key}
}
function Assert-MistwatchForestSurfaceFields($Value){
    $fixed=@{profile='forest-moss-leaf-metric-v1';
        ground_art_surface_path='moss-height/Ground037_SurfaceOrmHeight_Level0.png';ground_art_surface_bytes='1531546';
        ground_art_surface_sha256='012d2378aa6e62cca05ad66d69b512966d5894694b7b24d39afd0ca7ca655f38';ground_art_surface_encoding='2';
        ground_habitat_layer_surface_path='leaf-height/ForestLeaves04_SurfaceOrmHeight_Level0.png';ground_habitat_layer_surface_bytes='9102955';
        ground_habitat_layer_surface_sha256='9f3259f4772f294edca1de03fa4e8d7ed65c0924238b1a6d0b27b8fed7c0ef85';
        ground_habitat_layer_surface_encoding='2';ground_rock_surface_encoding='0';height_layer_mask='3';ground_soil_layer_scale='1.75';
        parallax_enabled='1';parallax_depth_meters='0.015'}
    Assert-MistwatchFallenExactKeys $Value (@($fixed.Keys)+@('ground_loaded_texture_identity','parallax_settings_fingerprint')) 'surface-only16 fields'
    foreach($key in $fixed.Keys){
        if($Value[$key] -isnot [string] -or $Value[$key] -cne $fixed[$key]){throw ('Surface fixed recipe differs: '+$key)}
    }
    foreach($key in @('ground_loaded_texture_identity','parallax_settings_fingerprint')){
        Assert-MistwatchAuthoredNativeUnsigned $Value[$key] $key
        if($Value[$key] -ceq '0'){throw ('Surface identity missing: '+$key)}
    }
}
function Assert-MistwatchForestSurfaceProvenance($Value){
    $keys=@('schema','world_cohort','authored_world_qualified','runtime_profile','generated_root_stem_override','current_geometry_qualified',
        'source_root','source_files','runtime_assets_root','runtime_assets','fallen_source_assets','raw_mesh','test_source',
        'cpu_binary','toolchain','source_input_unchanged','runtime_input_unchanged','outer_receipt_scope')
    Assert-MistwatchFallenExactKeys $Value $keys 'surface provenance'
    foreach($pair in @(@('schema','mistwatch.forest-surface.cpu-provenance.1'),@('world_cohort','entrance-authored-surface-only-v1'),
        @('runtime_profile','authored-forest-surface-full-v4'))){
        if($Value[$pair[0]] -isnot [string] -or $Value[$pair[0]] -cne $pair[1]){throw 'Surface provenance schema/cohort/profile differs'}
    }
    foreach($key in @('authored_world_qualified','current_geometry_qualified')){Assert-MistwatchAuthoredNativeBoolean $Value[$key] $false $key}
    foreach($key in @('source_input_unchanged','runtime_input_unchanged')){Assert-MistwatchAuthoredNativeBoolean $Value[$key] $true $key}
    foreach($key in @('source_root','runtime_assets_root')){
        if($Value[$key] -isnot [string] -or -not [IO.Path]::IsPathFullyQualified($Value[$key])){throw 'Surface provenance absolute root required'}
    }
    if($Value.generated_root_stem_override -isnot [string] -or -not [IO.Path]::IsPathFullyQualified($Value.generated_root_stem_override)){
        throw 'Surface generated root must be explicit and absolute'
    }
    foreach($key in @('source_files','runtime_assets','fallen_source_assets')){Assert-MistwatchAuthoredNativeRowsShape $Value[$key] $key}
    foreach($key in @('raw_mesh','test_source','cpu_binary')){Assert-MistwatchAuthoredNativeIdentity $Value[$key] $key}
    foreach($key in @('raw_mesh','test_source')){Assert-MistwatchAuthoredNativeRelative $Value[$key].path $key}
    if(-not [IO.Path]::IsPathFullyQualified($Value.cpu_binary.path)){throw 'Surface CPU binary absolute path required'}
    Assert-MistwatchFallenToolchain $Value.toolchain
    if($Value.outer_receipt_scope -isnot [string] -or $Value.outer_receipt_scope -cne
        'Before/after selected source and runtime raw identities only. Root independently binds fresh compile/link inputs, exits, Job drain and JUnit; no self-claimed outer process or GPU qualification.'){
        throw 'Surface provenance observation scope differs'
    }
}
function Assert-MistwatchForestSurfaceExpected($Value){
    Assert-MistwatchFallenExactKeys $Value @('schema','world_cohort','runtime_profile','surface_contract','provenance','scope') 'surface expected'
    foreach($pair in @(@('schema','mistwatch.forest-surface.cpu-expected.1'),@('world_cohort','entrance-authored-surface-only-v1'),
        @('runtime_profile','authored-forest-surface-full-v4'))){
        if($Value[$pair[0]] -isnot [string] -or $Value[$pair[0]] -cne $pair[1]){throw 'Surface expectation schema/cohort/profile differs'}
    }
    Assert-MistwatchForestSurfaceFields $Value.surface_contract
    Assert-MistwatchForestSurfaceProvenance $Value.provenance
    Assert-MistwatchForestSurfaceScope $Value.scope
}
function Read-MistwatchForestSurfaceCertificate {
    param([Parameter(Mandatory=$true)][string]$CertificateRoot,[Parameter(Mandatory=$true)][string]$SourceRoot,
        [Parameter(Mandatory=$true)][string]$BuildRoot,[Parameter(Mandatory=$true)][string]$ExecutablePath)
    $roots=Assert-MistwatchFallenBuildRoots $SourceRoot $BuildRoot
    $source=$roots.source;$build=$roots.build
    $exe=Assert-MistwatchFallenPhysicalPath $ExecutablePath
    if($exe -ine [IO.Path]::GetFullPath((Join-Path $build 'bin/Release/Aevrynd-Mistwatch.exe'))){throw 'Surface fixed build target required'}
    $assets=Assert-MistwatchFallenPhysicalPath (Join-Path (Split-Path $exe) 'assets') -Directory
    $certificate=Assert-MistwatchFallenPhysicalPath $CertificateRoot -Directory
    if(Test-Path -LiteralPath (Join-Path $certificate 'failure.txt')){throw 'Surface failed output cannot qualify'}
    $expected=Read-MistwatchFallenStrictJson (Join-Path $certificate 'SurfaceExpected.json')
    $receipt=Read-MistwatchFallenStrictJson (Join-Path $certificate 'receipt.json')
    Assert-MistwatchForestSurfaceExpected $expected
    Assert-MistwatchFallenExactKeys $receipt @('schema','world_cohort','runtime_profile','status','current_geometry_qualified',
        'native_expected_author_passed','source_input_unchanged','runtime_input_unchanged','expected_file','proof_file','cpu_binary','scope') 'surface receipt'
    foreach($pair in @(@('schema','mistwatch.forest-surface.cpu-receipt.1'),@('world_cohort','entrance-authored-surface-only-v1'),
        @('runtime_profile','authored-forest-surface-full-v4'),@('status','surface-runtime-CPU-passed'))){
        if($receipt[$pair[0]] -isnot [string] -or $receipt[$pair[0]] -cne $pair[1]){throw 'Surface receipt schema/status/cohort/profile differs'}
    }
    foreach($key in @('current_geometry_qualified','native_expected_author_passed')){Assert-MistwatchAuthoredNativeBoolean $receipt[$key] $false $key}
    foreach($key in @('source_input_unchanged','runtime_input_unchanged')){Assert-MistwatchAuthoredNativeBoolean $receipt[$key] $true $key}
    Assert-MistwatchForestSurfaceScope $receipt.scope
    Assert-MistwatchAuthoredNativeSameTree $receipt.scope $expected.scope 'surface sealed scope'
    foreach($pair in @(@('expected_file','SurfaceExpected.json'),@('proof_file','surface-proof.txt'))){
        Assert-MistwatchAuthoredNativeIdentity $receipt[$pair[0]] $pair[0]
        if($receipt[$pair[0]].path -cne $pair[1]){throw 'Surface fixed certificate output required'}
    }
    Assert-MistwatchFallenRelativeRows @($receipt.expected_file,$receipt.proof_file) $certificate 'surface sealed outputs'
    $p=$expected.provenance
    if([IO.Path]::GetFullPath($p.source_root) -ine $source -or [IO.Path]::GetFullPath($p.runtime_assets_root) -ine $assets){throw 'Surface provenance roots differ'}
    $generated=[IO.Path]::GetFullPath((Join-Path $build 'games/mistwatch/native/generated-root-stem'))
    if([IO.Path]::GetFullPath($p.generated_root_stem_override) -ine $generated){throw 'Surface generated producer outside current build'}
    $required=@('CMakeLists.txt','engine/native/CMakeLists.txt','games/mistwatch/native/CMakeLists.txt','games/mistwatch/native/src/main.cpp',
        'games/mistwatch/tools/author_root_stem.py','games/mistwatch/native/tests/MistwatchFallenWorldQualificationTest.cpp',
        'games/mistwatch/native/tests/fallen/MistwatchFallenSeatSupport.h','games/mistwatch/native/tests/fallen/MistwatchFallenSourceSupport.h',
        'games/mistwatch/native/tests/fallen/MistwatchQualificationRuntime.h','games/mistwatch/native/tests/qualification/MistwatchQualificationRuntimeTest.cpp',
        'games/mistwatch/native/tests/qualification/runtime-profiles-v1.json','games/mistwatch/native/tests/fallen/MistwatchAuthoredWorldQualification.h',
        'games/mistwatch/native/tests/qualification/MistwatchAuthoredWorldQualificationTest.cpp','games/mistwatch/native/tests/fallen/MistwatchAuthoredNativeExpected.h',
        'games/mistwatch/native/tests/qualification/MistwatchAuthoredNativeExpectedTest.cpp','games/mistwatch/tools/AuthoredNativeContract.ps1',
        'games/mistwatch/native/tests/MistwatchAuthoredNativeReceiptTest.ps1','games/mistwatch/tools/ForestSurfaceContract.ps1',
        'games/mistwatch/native/tests/qualification/runtime-leaf-cluster-full-v3.json','games/mistwatch/native/tests/qualification/runtime-forest-surface-full-v4.json')
    foreach($entry in (Get-MistwatchLeafClusterRuntimeAdditions $source)+(Get-MistwatchForestSurfaceRuntimeAdditions $source)){$required+=[string]$entry[1]}
    foreach($pin in Get-MistwatchFallenPackagePins){$required+='engine/assets/mistwatch-fallen-logs/'+$pin[0]}
    $wanted=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach($path in $required){$null=$wanted.Add($path)}
    foreach($directory in @('games/mistwatch/native/include','engine/native/include','engine/native/src','engine/native/shaders',
        'engine/assets/mistwatch-nettle-plant','engine/assets/material-packs/poly-haven/forest_leaves_04')){
        $dir=Assert-MistwatchFallenPhysicalPath (Join-Path $source $directory) -Directory
        foreach($file in Get-ChildItem -LiteralPath $dir -Recurse -Force){
            if($file.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Surface source closure reparse'}
            if($file -is [IO.DirectoryInfo]){continue}
            if($file -isnot [IO.FileInfo]){throw 'Surface source regular file required'}
            $null=$wanted.Add([IO.Path]::GetRelativePath($source,$file.FullName).Replace('\','/'))
        }
    }
    if($p.source_files.Count -ne $wanted.Count){throw 'Surface exact source closure count differs'}
    foreach($row in $p.source_files){if(-not $wanted.Contains($row.path)){throw 'Surface unexpected source member'}}
    Assert-MistwatchFallenRelativeRows $p.source_files $source 'surface source closure'
    if($p.raw_mesh.path -cne 'engine/assets/mistwatch-fallen-logs/meshes/dead-tree-trunk-lod2-node0-mesh0-primitive0-materialnone-material-none.aemesh' -or
       $p.test_source.path -cne 'games/mistwatch/native/tests/MistwatchFallenWorldQualificationTest.cpp'){throw 'Surface raw/test identity differs'}
    foreach($row in @($p.raw_mesh,$p.test_source)){
        $match=@($p.source_files|Where-Object{$_.path -ceq $row.path})
        if($match.Count -ne 1){throw 'Surface source subset missing'}
        Assert-MistwatchAuthoredNativeSameTree $row $match[0] 'surface sealed source subset'
    }
    if($p.fallen_source_assets.Count -ne 6){throw 'Surface fallen6 required'}
    foreach($pin in Get-MistwatchFallenPackagePins){
        $relative='engine/assets/mistwatch-fallen-logs/'+$pin[0]
        $match=@($p.fallen_source_assets|Where-Object{$_.path -ceq $relative})
        if($match.Count -ne 1 -or $match[0].bytes -ne $pin[1] -or $match[0].sha256 -cne $pin[2]){throw 'Surface fallen6 pin differs'}
    }
    Assert-MistwatchFallenRelativeRows $p.fallen_source_assets $source 'surface fallen6'
    foreach($row in @($p.cpu_binary,$receipt.cpu_binary)){
        Assert-MistwatchFallenRow $row (Join-Path $build 'bin/Release/MistwatchFallenWorldQualificationTest.exe')
    }
    Assert-MistwatchAuthoredNativeSameTree $p.cpu_binary $receipt.cpu_binary 'surface CPU binary identity'
    $actual=Get-MistwatchAuthoredNativeRuntimeRows $source $assets 'authored-forest-surface-full-v4' $generated
    Assert-MistwatchFallenRelativeRows $p.runtime_assets $assets 'surface current runtime418'
    $a=@($actual|Sort-Object path|ForEach-Object{$_.path+'|'+$_.bytes+'|'+$_.sha256})
    $b=@($p.runtime_assets|Sort-Object path|ForEach-Object{$_.path+'|'+$_.bytes+'|'+$_.sha256})
    if(($a-join "`n") -cne ($b-join "`n")){throw 'Surface runtime418 certificate membership differs'}
    return ,$expected
}
