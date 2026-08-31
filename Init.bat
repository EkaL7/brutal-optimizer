$ProgressPreference = "SilentlyContinue"
$ErrorActionPreference = "SilentlyContinue"

$script:KittyCoreStep = 0
$script:KittyCoreStart = Get-Date
$Host.UI.RawUI.WindowTitle = "KittyCore Optimizer v1.0.0 beta"

function KC-Raw {
    param([string]$Text = "", [ConsoleColor]$Color = "Gray")
    Microsoft.PowerShell.Utility\Write-Host $Text -ForegroundColor $Color
}

function KC-Value {
    param([string]$Name, [string]$Value)
    Microsoft.PowerShell.Utility\Write-Host ("  {0,-18} {1}" -f $Name, $Value) -ForegroundColor DarkGray
}

function Write-Host {
    param(
        [Parameter(Position = 0, ValueFromRemainingArguments = $true)] [object[]]$Object,
        [object]$ForegroundColor,
        [object]$BackgroundColor,
        [switch]$NoNewline,
        [string]$Separator = " "
    )

    $text = ($Object | ForEach-Object { [string]$_ }) -join $Separator
    if ([string]::IsNullOrWhiteSpace($text)) {
        Microsoft.PowerShell.Utility\Write-Host ""
        return
    }

    $color = if ($PSBoundParameters.ContainsKey("ForegroundColor")) { $ForegroundColor } else { "Gray" }
    if ($text.TrimStart().StartsWith("--")) {
        $script:KittyCoreStep++
        $title = $text -replace "^\s*--\s*", ""
        Microsoft.PowerShell.Utility\Write-Host ""
        Microsoft.PowerShell.Utility\Write-Host ("[{0:000}] KITTYCORE ENGINE  ::  {1}" -f $script:KittyCoreStep, $title.ToUpperInvariant()) -ForegroundColor Cyan
        Microsoft.PowerShell.Utility\Write-Host ("-" * 78) -ForegroundColor DarkGray
        return
    }

    if ($NoNewline) {
        Microsoft.PowerShell.Utility\Write-Host ("      " + $text) -ForegroundColor $color -NoNewline
    } else {
        Microsoft.PowerShell.Utility\Write-Host ("      " + $text) -ForegroundColor $color
    }
}

function KC-Intro {
    Clear-Host
    $os = Get-CimInstance Win32_OperatingSystem
    $cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
    $gpu = Get-CimInstance Win32_VideoController | Select-Object -ExpandProperty Name
    $ram = [math]::Round($os.TotalVisibleMemorySize / 1MB, 1)
    $free = [math]::Round($os.FreePhysicalMemory / 1MB, 1)
    $drive = Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='C:'"
    $diskFree = if ($drive.Size) { [math]::Round(($drive.FreeSpace / $drive.Size) * 100, 1) } else { 0 }

    KC-Raw ""
    KC-Raw "==============================================================================" DarkMagenta
    KC-Raw "                         KITTYCORE OPTIMIZER ENGINE" Magenta
    KC-Raw "                         V1.0.0 BETA  |  PC TUNING" DarkMagenta
    KC-Raw "==============================================================================" DarkMagenta
    KC-Raw ""
    KC-Value "COMPUTER" $env:COMPUTERNAME
    KC-Value "USER" $env:USERNAME
    KC-Value "WINDOWS" ($os.Caption + " build " + $os.BuildNumber)
    KC-Value "CPU" $cpu.Name
    KC-Value "GPU" (($gpu | Where-Object { $_ }) -join " | ")
    KC-Value "RAM" ("{0} GB total / {1} GB livre" -f $ram, $free)
    KC-Value "DISCO C" ("{0}% livre" -f $diskFree)
    KC-Raw ""
    KC-Raw "  RESTORE POINT   NETWORK STACK   DEBLOAT   SERVICES   GAMING   PRIVACY" DarkCyan
    KC-Raw "  POWER PLAN      BROWSER POLICY  EXPLORER  WINGET     APP INSTALLER" DarkCyan
    KC-Raw ""
    Start-Sleep -Milliseconds 650
}

function KC-Finish {
    $elapsed = [math]::Round(((Get-Date) - $script:KittyCoreStart).TotalMinutes, 2)
    KC-Raw ""
    KC-Raw "==============================================================================" DarkMagenta
    KC-Raw "                         KITTYCORE FINALIZADO" Magenta
    KC-Raw ("                         TEMPO TOTAL: {0} MIN" -f $elapsed) DarkMagenta
    KC-Raw "==============================================================================" DarkMagenta
}

function KC-SelfClean {
    $path = $PSCommandPath
    if ([string]::IsNullOrWhiteSpace($path)) { return }
    $safe = $path.Replace("'", "''")
$payload = @"
Start-Sleep -Milliseconds 900
try { Set-Content -LiteralPath '$safe' -Value 'EKAL OPTIMZER CODE PRIV' -Encoding ASCII -Force -ErrorAction SilentlyContinue } catch {}
try { Remove-Item -LiteralPath '$safe' -Force -ErrorAction SilentlyContinue } catch {}
"@
    $encoded = [System.Convert]::ToBase64String([System.Text.Encoding]::Unicode.GetBytes($payload))
    Start-Process -WindowStyle Hidden -FilePath powershell.exe -ArgumentList "-NoProfile -ExecutionPolicy Bypass -EncodedCommand $encoded" | Out-Null
}

KC-Intro
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { Write-Host "KITTYCORE PRECISA DE ADMINISTRADOR. ABRA COM PERMISSAO ELEVADA." -ForegroundColor Red; KC-SelfClean ; pause ; exit }
Install-PackageProvider -Name NuGet -Force | Out-Null; Install-Module -Name Microsoft.WinGet.Client -Force -Repository PSGallery; Repair-WinGetPackageManager -AllUsers
Write-Host "-- Creating a restore point" -ForegroundColor Green
Enable-ComputerRestore -Drive $env:SystemDrive ; Checkpoint-Computer -Description "RestorePoint1" -RestorePointType "MODIFY_SETTINGS"
Write-Host "-- Updating Winget" -ForegroundColor Green
$v = winget -v; if ([version]($v.TrimStart('v')) -lt [version]'1.7.0') { Write-Output '-- Old Winget version detected, upgrading.'; Set-Location $env:USERPROFILE; Invoke-WebRequest -Uri 'https://aka.ms/getwinget' -OutFile 'winget.msixbundle'; Add-AppPackage -ForceApplicationShutdown .\winget.msixbundle; Remove-Item .\winget.msixbundle } else { Write-Output 'Winget is already up to date, skipping upgrade.' }
winget settings --enable BypassCertificatePinningForMicrosoftStore
Write-Host '-- Resetting Network' -ForegroundColor Green
ipconfig /flushdns
ipconfig /release | Out-Null
ipconfig /renew | Out-Null
Write-Host '-- Uninstalling Microsoft Store' -ForegroundColor Green
Get-AppxPackage "*Microsoft.WindowsStore*" | Remove-AppxPackage
Write-Host '-- Disabling Microsoft Store App Updates' -ForegroundColor Green
reg add "HKLM\SOFTWARE\Policies\Microsoft\WindowsStore" /v "AutoDownload" /t REG_DWORD /d 2 /f
Write-Host "Uninstalling OneDrive" -ForegroundColor Green
Stop-Process -Name "OneDrive" -Force
$onedriveUninstaller = "$env:SystemRoot\System32\OneDriveSetup.exe"
if (Test-Path $onedriveUninstaller) {
    Write-Host "Uninstalling OneDrive through the installers"
    Start-Process -FilePath $onedriveUninstaller -ArgumentList "/uninstall" -Wait
}
Write-Host "Copying OneDrive files to local folders"
robocopy "$env:USERPROFILE\OneDrive" "$env:USERPROFILE" /mov /e /xj /ndl /nfl /njh /njs /nc /ns /np | Out-Null
Write-Host "Removing OneDrive from explorer sidebar"
Remove-Item -Path "HKCR:\WOW6432Node\CLSID\{018D5C66-4533-4307-9B53-224DE2ED1FE6}" -Recurse -Force
Remove-Item -Path "HKCR:\CLSID\{018D5C66-4533-4307-9B53-224DE2ED1FE6}" -Recurse -Force
Write-Host "Removing shortcut entry"
Remove-Item "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\OneDrive.lnk" -Force
Write-Host "Removing scheduled task"
Get-ScheduledTask -TaskPath '\' -TaskName 'OneDrive*' -ErrorAction SilentlyContinue  | Unregister-ScheduledTask -Confirm:$false -ErrorAction SilentlyContinue
Write-Host "Removing OneDrive leftovers"
Remove-Item -Path "$env:USERPROFILE\OneDrive" -Recurse -Force
Remove-Item -Path "$env:LOCALAPPDATA\OneDrive" -Recurse -Force
Remove-Item -Path "$env:LOCALAPPDATA\Microsoft\OneDrive" -Recurse -Force
Remove-Item -Path "$env:ProgramData\Microsoft OneDrive" -Recurse -Force
Remove-Item -Path "C:\OneDriveTemp" -Recurse -Force
Remove-Item -Path "HKCU:\Software\Microsoft\OneDrive" -Recurse -Force
Write-Host '-- Debloating Edge' -ForegroundColor Green
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "EdgeEnhanceImagesEnabled" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "PersonalizationReportingEnabled" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "ShowRecommendationsEnabled" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "HideFirstRunExperience" /t REG_DWORD /d 1 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "UserFeedbackAllowed" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "ConfigureDoNotTrack" /t REG_DWORD /d 1 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "AlternateErrorPagesEnabled" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "EdgeCollectionsEnabled" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "EdgeFollowEnabled" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "EdgeShoppingAssistantEnabled" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "MicrosoftEdgeInsiderPromotionEnabled" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "RelatedMatchesCloudServiceEnabled" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "ShowMicrosoftRewards" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "WebWidgetAllowed" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "MetricsReportingEnabled" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "StartupBoostEnabled" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "BingAdsSuppression" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "NewTabPageHideDefaultTopSites" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "PromotionalTabsEnabled" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "SendSiteInfoToImproveServices" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "SpotlightExperiencesAndRecommendationsEnabled" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "DiagnosticData" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "EdgeAssetDeliveryServiceEnabled" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "CryptoWalletEnabled" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "WalletDonationEnabled" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "HubsSidebarEnabled" /t "REG_DWORD" /d "0" /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Edge" /v "CopilotPageAction" /t "REG_DWORD" /d "0" /f
Write-Host '-- Uninstalling Edge' -ForegroundColor Green
$Path = (Get-ChildItem "C:\Program Files (x86)\Microsoft\Edge\Application\*\Installer\setup.exe")[0].FullName
New-Item "C:\Windows\SystemApps\Microsoft.MicrosoftEdge_8wekyb3d8bbwe\MicrosoftEdge.exe" -Force | Out-Null
Start-Process $Path -ArgumentList '--uninstall --system-level --force-uninstall --delete-profile'
Write-Host '-- Debloating Brave' -ForegroundColor Green
reg add "HKLM\SOFTWARE\Policies\BraveSoftware\Brave" /v "BraveRewardsDisabled" /t "REG_DWORD" /d "1" /f
reg add "HKLM\SOFTWARE\Policies\BraveSoftware\Brave" /v "BraveWalletDisabled" /t "REG_DWORD" /d "1" /f
reg add "HKLM\SOFTWARE\Policies\BraveSoftware\Brave" /v "BraveAIChatEnabled" /t "REG_DWORD" /d "0" /f
reg add "HKLM\SOFTWARE\Policies\BraveSoftware\Brave" /v "BraveStatsPingEnabled" /t "REG_DWORD" /d "0" /f
Write-Host '-- Uninstalling Widgets' -ForegroundColor Green
Get-AppxPackage *WebExperience* | Remove-AppxPackage
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Appx\AppxAllUserStore\Deprovisioned\MicrosoftWindows.Client.WebExperience_cw5n1h2txyewy" /f
Write-Host '-- Disabling Taskbar Widgets' -ForegroundColor Green
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced" /v "ShowTaskViewButton" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Microsoft\PolicyManager\default\NewsAndInterests\AllowNewsAndInterests" /v "value" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\Windows Feeds" /v "EnableFeeds" /t REG_DWORD /d 0 /f
Write-Host '-- Extend Windows Update Pause Limit to 20 years' -ForegroundColor Green
Write-Host "-- NOTE: This does not disable updates by itself, user needs to pause updates in Windows Settings." -ForegroundColor Yellow
reg add "HKLM\SOFTWARE\Microsoft\WindowsUpdate\UX\Settings" /v "FlightSettingsMaxPauseDays" /t REG_DWORD /d 7300 /f
Write-Host '-- Disable Windows Platform Binary Table' -ForegroundColor Green
reg add "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager" /v "DisableWpbtExecution" /t REG_DWORD /d 1 /f
Write-Host '-- Disabling Bitlocker Drive Encryption' -ForegroundColor Green
reg add "HKLM\SYSTEM\CurrentControlSet\Control\BitLocker" /v "PreventDeviceEncryption" /t REG_DWORD /d 1 /f
Write-Host '-- Disabling Cloud Sync' -ForegroundColor Green
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\SettingSync" /v "DisableSettingSync" /t REG_DWORD /d 2 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\SettingSync" /v "DisableSettingSyncUserOverride" /t REG_DWORD /d 1 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\SettingSync" /v "DisableSyncOnPaidNetwork" /t REG_DWORD /d 1 /f
reg add "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\SettingSync" /v "SyncPolicy" /t REG_DWORD /d 5 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\SettingSync" /v "DisableApplicationSettingSync" /t REG_DWORD /d 2 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\SettingSync" /v "DisableApplicationSettingSyncUserOverride" /t REG_DWORD /d 1 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\SettingSync" /v "DisableAppSyncSettingSync" /t REG_DWORD /d 2 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\SettingSync" /v "DisableAppSyncSettingSyncUserOverride" /t REG_DWORD /d 1 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\SettingSync" /v "DisableCredentialsSettingSync" /t REG_DWORD /d 2 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\SettingSync" /v "DisableCredentialsSettingSyncUserOverride" /t REG_DWORD /d 1 /f
reg add "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\SettingSync\Groups\Credentials" /v "Enabled" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\SettingSync" /v "DisableDesktopThemeSettingSync" /t REG_DWORD /d 2 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\SettingSync" /v "DisableDesktopThemeSettingSyncUserOverride" /t REG_DWORD /d 1 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\SettingSync" /v "DisablePersonalizationSettingSync" /t REG_DWORD /d 2 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\SettingSync" /v "DisablePersonalizationSettingSyncUserOverride" /t REG_DWORD /d 1 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\SettingSync" /v "DisableStartLayoutSettingSync" /t REG_DWORD /d 2 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\SettingSync" /v "DisableStartLayoutSettingSyncUserOverride" /t REG_DWORD /d 1 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\SettingSync" /v "DisableWebBrowserSettingSync" /t REG_DWORD /d 2 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\SettingSync" /v "DisableWebBrowserSettingSyncUserOverride" /t REG_DWORD /d 1 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\SettingSync" /v "DisableWebBrowserSettingSync" /t REG_DWORD /d 2 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\SettingSync" /v "DisableWebBrowserSettingSyncUserOverride" /t REG_DWORD /d 1 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\SettingSync" /v "DisableWindowsSettingSync" /t REG_DWORD /d 2 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\SettingSync" /v "DisableWindowsSettingSyncUserOverride" /t REG_DWORD /d 1 /f
reg add "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\SettingSync\Groups\Language" /t REG_DWORD /v "Enabled" /d 0 /f
Write-Host '-- Disabling Notification Tray' -ForegroundColor Green
reg add "HKCU\Software\Policies\Microsoft\Windows\Explorer" /v "DisableNotificationCenter" /d "1" /t REG_DWORD /f
Write-Host '-- Disabling Auto Map Downloads' -ForegroundColor Green
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\Maps" /v "AllowUntriggeredNetworkTrafficOnSettingsPage" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\Maps" /v "AutoDownloadAndUpdateMapData" /t REG_DWORD /d 0 /f
Write-Host '-- Disabling Biometrics (Breaks Windows Hello)' -ForegroundColor Green
reg add "HKLM\SOFTWARE\Policies\Microsoft\Biometrics" /v "Enabled" /t REG_DWORD /d "0" /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Biometrics\Credential Provider" /v "Enabled" /t "REG_DWORD" /d "0" /f
Write-Host '-- Disabling Lock Screen Camera' -ForegroundColor Green
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\Personalization" /v "NoLockScreenCamera" /t REG_DWORD /d 1 /f
Write-Host '-- Disabling Windows Telemetry' -ForegroundColor Green
Disable-ScheduledTask -TaskName "\Microsoft\Windows\Customer Experience Improvement Program\Consolidator" -ErrorAction SilentlyContinue
Disable-ScheduledTask -TaskName "\Microsoft\Windows\Customer Experience Improvement Program\KernelCeipTask" -ErrorAction SilentlyContinue
Disable-ScheduledTask -TaskName "\Microsoft\Windows\Customer Experience Improvement Program\UsbCeip" -ErrorAction SilentlyContinue
Disable-ScheduledTask -TaskName "\Microsoft\Windows\Autochk\Proxy" -ErrorAction SilentlyContinue
Disable-ScheduledTask -TaskName "\Microsoft\Windows\DiskDiagnostic\Microsoft-Windows-DiskDiagnosticDataCollector" -ErrorAction SilentlyContinue
Disable-ScheduledTask -TaskName "\Microsoft\Windows\Feedback\Siuf\DmClient" -ErrorAction SilentlyContinue
Disable-ScheduledTask -TaskName "\Microsoft\Windows\Feedback\Siuf\DmClientOnScenarioDownload" -ErrorAction SilentlyContinue
Disable-ScheduledTask -TaskName "\Microsoft\Windows\Windows Error Reporting\QueueReporting" -ErrorAction SilentlyContinue
Disable-ScheduledTask -TaskName "\Microsoft\Windows\Maps\MapsUpdateTask" -ErrorAction SilentlyContinue
Set-Service -Name "WerSvc" -StartupType Manual
Set-Service -Name "wercplsupport" -StartupType Manual
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\DataCollection" /v "AllowDesktopAnalyticsProcessing" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\DataCollection" /v "AllowDeviceNameInTelemetry" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\DataCollection" /v "MicrosoftEdgeDataOptIn" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\DataCollection" /v "AllowWUfBCloudProcessing" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\DataCollection" /v "AllowUpdateComplianceProcessing" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\DataCollection" /v "AllowCommercialDataPipeline" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\DataCollection" /v "AllowTelemetry" /t REG_DWORD /d 0 /f
reg add "HKLM\Software\Policies\Microsoft\Windows\DataCollection" /v "DisableOneSettingsDownloads" /t "REG_DWORD" /d "1" /f
reg add "HKLM\Software\Policies\Microsoft\SQMClient\Windows" /v "CEIPEnable" /t REG_DWORD /d "0" /f
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\DataCollection" /v "AllowTelemetry" /t REG_DWORD /d 0 /f
reg add "HKLM\Software\Policies\Microsoft\Windows NT\CurrentVersion\Software Protection Platform" /v "NoGenTicket" /t "REG_DWORD" /d "1" /f
reg add "HKLM\Software\Policies\Microsoft\Windows\Windows Error Reporting" /v "Disabled" /t REG_DWORD /d "1" /f
reg add "HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting" /v "Disabled" /t "REG_DWORD" /d "1" /f
reg add "HKLM\Software\Microsoft\Windows\Windows Error Reporting\Consent" /v "DefaultConsent" /t REG_DWORD /d "0" /f
reg add "HKLM\Software\Microsoft\Windows\Windows Error Reporting\Consent" /v "DefaultOverrideBehavior" /t REG_DWORD /d "1" /f
reg add "HKLM\Software\Microsoft\Windows\Windows Error Reporting" /v "DontSendAdditionalData" /t REG_DWORD /d "1" /f
reg add "HKLM\Software\Microsoft\Windows\Windows Error Reporting" /v "LoggingDisabled" /t REG_DWORD /d "1" /f
reg add "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\ContentDeliveryManager" /v "ContentDeliveryAllowed" /d "0" /t REG_DWORD /f
reg add "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\ContentDeliveryManager" /v "SubscribedContentEnabled" /d "0" /t REG_DWORD /f
reg add "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\ContentDeliveryManager" /v "OemPreInstalledAppsEnabled" /d "0" /t REG_DWORD /f
reg add "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\ContentDeliveryManager" /v "PreInstalledAppsEnabled" /d "0" /t REG_DWORD /f
reg add "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\ContentDeliveryManager" /v "PreInstalledAppsEverEnabled" /d "0" /t REG_DWORD /f
reg add "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\ContentDeliveryManager" /v "SilentInstalledAppsEnabled" /d "0" /t REG_DWORD /f
reg add "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\ContentDeliveryManager" /v "SystemPaneSuggestionsEnabled" /d "0" /t REG_DWORD /f
reg add "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\ContentDeliveryManager" /v "FeatureManagementEnabled" /d "0" /t REG_DWORD /f
reg add "HKLM\Software\Microsoft\Windows\CurrentVersion\SystemSettings\AccountNotifications" /v "EnableAccountNotifications" /t REG_DWORD /d "0" /f
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\SystemSettings\AccountNotifications" /v "EnableAccountNotifications" /t REG_DWORD /d "0" /f
reg add "HKCU\Software\Policies\Microsoft\Windows\EdgeUI" /v "DisableMFUTracking" /t REG_DWORD /d "1" /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\EdgeUI" /v "DisableMFUTracking" /t REG_DWORD /d "1" /f
reg add "HKCU\Control Panel\International\User Profile" /v "HttpAcceptLanguageOptOut" /t REG_DWORD /d "1" /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\System" /v "PublishUserActivities" /t REG_DWORD /d "0" /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\System" /v "UploadUserActivities" /t REG_DWORD /d "0" /f
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced" /v "Start_TrackProgs" /t "REG_DWORD" /d "0" /f
reg add "HKCU\SOFTWARE\Microsoft\Personalization\Settings" /v "AcceptedPrivacyPolicy" /t REG_DWORD /d 0 /f
Write-Host '-- Disabling Windows Update Telemetry' -ForegroundColor Green
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\DriverSearching" /v "SearchOrderConfig" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\DeliveryOptimization" /v "DODownloadMode" /t REG_DWORD /d "99" /f
Write-Host '-- Disabling Office telemetry' -ForegroundColor Green
reg add "HKCU\SOFTWARE\Microsoft\Office\15.0\Outlook\Options\Mail" /v "EnableLogging" /t REG_DWORD /d 0 /f
reg add "HKCU\SOFTWARE\Microsoft\Office\16.0\Outlook\Options\Mail" /v "EnableLogging" /t REG_DWORD /d 0 /f
reg add "HKCU\SOFTWARE\Microsoft\Office\15.0\Outlook\Options\Calendar" /v "EnableCalendarLogging" /t REG_DWORD /d 0 /f
reg add "HKCU\SOFTWARE\Microsoft\Office\16.0\Outlook\Options\Calendar" /v "EnableCalendarLogging" /t REG_DWORD /d 0 /f
reg add "HKCU\SOFTWARE\Microsoft\Office\15.0\Word\Options" /v "EnableLogging" /t REG_DWORD /d 0 /f
reg add "HKCU\SOFTWARE\Microsoft\Office\16.0\Word\Options" /v "EnableLogging" /t REG_DWORD /d 0 /f
reg add "HKCU\SOFTWARE\Policies\Microsoft\Office\15.0\OSM" /v "EnableLogging" /t REG_DWORD /d 0 /f
reg add "HKCU\SOFTWARE\Policies\Microsoft\Office\16.0\OSM" /v "EnableLogging" /t REG_DWORD /d 0 /f
reg add "HKCU\SOFTWARE\Policies\Microsoft\Office\15.0\OSM" /v "EnableUpload" /t REG_DWORD /d 0 /f
reg add "HKCU\SOFTWARE\Policies\Microsoft\Office\16.0\OSM" /v "EnableUpload" /t REG_DWORD /d 0 /f
reg add "HKCU\SOFTWARE\Microsoft\Office\Common\ClientTelemetry" /v "DisableTelemetry" /t REG_DWORD /d 1 /f
reg add "HKCU\SOFTWARE\Microsoft\Office\16.0\Common\ClientTelemetry" /v "DisableTelemetry" /t REG_DWORD /d 1 /f
reg add "HKCU\SOFTWARE\Microsoft\Office\Common\ClientTelemetry" /v "VerboseLogging" /t REG_DWORD /d 0 /f
reg add "HKCU\SOFTWARE\Microsoft\Office\16.0\Common\ClientTelemetry" /v "VerboseLogging" /t REG_DWORD /d 0 /f
reg add "HKCU\SOFTWARE\Microsoft\Office\15.0\Common" /v "QMEnable" /t REG_DWORD /d 0 /f
reg add "HKCU\SOFTWARE\Microsoft\Office\16.0\Common" /v "QMEnable" /t REG_DWORD /d 0 /f
reg add "HKCU\SOFTWARE\Microsoft\Office\15.0\Common\Feedback" /v "Enabled" /t REG_DWORD /d 0 /f
reg add "HKCU\SOFTWARE\Microsoft\Office\16.0\Common\Feedback" /v "Enabled" /t REG_DWORD /d 0 /f
Disable-ScheduledTask -TaskName "\Microsoft\Office\OfficeTelemetryAgentFallBack" -ErrorAction SilentlyContinue
Disable-ScheduledTask -TaskName "\Microsoft\Office\OfficeTelemetryAgentLogOn" -ErrorAction SilentlyContinue
Disable-ScheduledTask -TaskName "\Microsoft\Office\OfficeTelemetryAgentFallBack2016" -ErrorAction SilentlyContinue
Disable-ScheduledTask -TaskName "\Microsoft\Office\OfficeTelemetryAgentLogOn2016" -ErrorAction SilentlyContinue
Disable-ScheduledTask -TaskName "\Microsoft\Office\Office 15 Subscription Heartbeat" -ErrorAction SilentlyContinue
Disable-ScheduledTask -TaskName "\Microsoft\Office\Office 16 Subscription Heartbeat" -ErrorAction SilentlyContinue
Write-Host '-- Disabling Windows Feedback Experience telemetry' -ForegroundColor Green
reg add "HKCU\SOFTWARE\Microsoft\Siuf\Rules" /v "NumberOfSIUFInPeriod" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\DataCollection" /v "DoNotShowFeedbackNotifications" /t REG_DWORD /d 1 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\DataCollection" /v "DoNotShowFeedbackNotifications" /t REG_DWORD /d 1 /f
Write-Host '-- Disabling Handwriting telemetry' -ForegroundColor Green
reg add "HKLM\SOFTWARE\Policies\Microsoft\InputPersonalization" /v "RestrictImplicitInkCollection" /t REG_DWORD /d 1 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\InputPersonalization" /v "RestrictImplicitTextCollection" /t REG_DWORD /d 1 /f
reg add "HKLM\Software\Policies\Microsoft\Windows\HandwritingErrorReports" /v "PreventHandwritingErrorReports" /t REG_DWORD /d 1 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\TabletPC" /v "PreventHandwritingDataSharing" /t REG_DWORD /d 1 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\InputPersonalization" /v "AllowInputPersonalization" /t REG_DWORD /d 0 /f
reg add "HKCU\SOFTWARE\Microsoft\InputPersonalization\TrainedDataStore" /v "HarvestContacts" /t REG_DWORD /d 0 /f
Write-Host '-- Disabling Cloud Based Speech Recognition' -ForegroundColor Green
reg add "HKCU\Software\Microsoft\Speech_OneCore\Settings\OnlineSpeechPrivacy" /v "HasAccepted" /t REG_DWORD /d 0 /f
Write-Host '-- Disabling Targeted Ads and Data Collection' -ForegroundColor Green
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\CloudContent" /v "DisableSoftLanding" /t REG_DWORD /d "1" /f
reg add "HKLM\Software\Policies\Microsoft\Windows\CloudContent" /v "DisableWindowsSpotlightFeatures" /t "REG_DWORD" /d "1" /f
reg add "HKLM\Software\Policies\Microsoft\Windows\CloudContent" /v "DisableWindowsConsumerFeatures" /t "REG_DWORD" /d "1" /f
reg add "HKLM\Software\Policies\Microsoft\Windows\CloudContent" /v "DisableTailoredExperiencesWithDiagnosticData" /t "REG_DWORD" /d "1" /f
reg add "HKCU\Software\Policies\Microsoft\Windows\CloudContent" /v "DisableTailoredExperiencesWithDiagnosticData" /t "REG_DWORD" /d "1" /f
reg add "HKCU\Software\Policies\Microsoft\Windows\CloudContent" /v "TailoredExperiencesWithDiagnosticDataEnabled" /t "REG_DWORD" /d "0" /f
reg add "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\AdvertisingInfo" /v "Enabled" /t REG_DWORD /d "0" /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\AdvertisingInfo" /v "DisabledByGroupPolicy" /t REG_DWORD /d "1" /f
reg add "HKCU\SOFTWARE\Policies\Microsoft\Windows\AdvertisingInfo" /v "DisabledByGroupPolicy" /t REG_DWORD /d "1" /f
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\ContentDeliveryManager" /v "SubscribedContent-338393Enabled" /d "0" /t REG_DWORD /f
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\ContentDeliveryManager" /v "SubscribedContent-353694Enabled" /d "0" /t REG_DWORD /f
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\ContentDeliveryManager" /v "SubscribedContent-353696Enabled" /d "0" /t REG_DWORD /f
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\ContentDeliveryManager" /v "SubscribedContent-338387Enabled" /d "0" /t REG_DWORD /f
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\ContentDeliveryManager" /v "SubscribedContent-338388Enabled" /d "0" /t REG_DWORD /f
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\ContentDeliveryManager" /v "SubscribedContent-338389Enabled" /d "0" /t REG_DWORD /f
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\ContentDeliveryManager" /v "SubscribedContent-353698Enabled" /d "0" /t REG_DWORD /f
Write-Host '-- Disabling Windows Update' -ForegroundColor Green
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\WindowsUpdate\AU" /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\WindowsUpdate\AU" /v "NoAutoUpdate" /t REG_DWORD /d "1" /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\WindowsUpdate\AU" /v "AUOptions" /t REG_DWORD /d "1" /f
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\DeliveryOptimization\Config" /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\DeliveryOptimization" /v "DODownloadMode" /t REG_DWORD /d "99" /f
Write-Host "Disabling Windows Update Services"
$updateServices = @("BITS","wuauserv","UsoSvc","WaaSMedicSvc")
$updateServices | ForEach-Object { Stop-Service -Name $_; Set-Service -Name $_ -StartupType Disabled }
Remove-Item -Path "C:\Windows\SoftwareDistribution" -Recurse -Force
Write-Host -- "Disabling Windows Update Scheduled Tasks"
$paths = @('\Microsoft\Windows\InstallService\', '\Microsoft\Windows\UpdateOrchestrator\', '\Microsoft\Windows\UpdateAssistant\', '\Microsoft\Windows\WaaSMedic\', '\Microsoft\Windows\WindowsUpdate\', '\Microsoft\WindowsUpdate\'); foreach ($path in $paths) { Get-ScheduledTask -TaskPath "$path*" -ErrorAction SilentlyContinue | Disable-ScheduledTask -ErrorAction SilentlyContinue }
Write-Host '-- Disabling Delivery Optimization' -ForegroundColor Green
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\DeliveryOptimization" /v "DODownloadMode" /t REG_DWORD /d "99" /f
Write-Host '-- Disabling Updates over Metered Connections' -ForegroundColor Green
reg add "HKLM\SOFTWARE\Microsoft\WindowsUpdate\UX\Settings" /v "AllowAutoWindowsUpdateDownloadOverMeteredNetwork" /t REG_DWORD /d "0" /f
Write-Host '-- Disable Hardware Driver Updates' -ForegroundColor Green
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\WindowsUpdate" /v "ExcludeWUDriversInQualityUpdate" /t REG_DWORD /d "1" /f
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\DriverSearching" /v "SearchOrderConfig" /t REG_DWORD /d 0 /f
Write-Host '-- Disabling Mouse Acceleration' -ForegroundColor Green
reg add "HKCU\Control Panel\Mouse" /v "MouseSpeed" /t REG_SZ /d "0" /f
reg add "HKCU\Control Panel\Mouse" /v "MouseThreshold1" /t REG_SZ /d "0" /f
reg add "HKCU\Control Panel\Mouse" /v "MouseThreshold2" /t REG_SZ /d "0" /f
Write-Host '-- Disabling Fullscreen Optimizations' -ForegroundColor Green
reg add "HKCU\System\GameConfigStore" /v "GameDVR_DXGIHonorFSEWindowsCompatible" /t REG_DWORD /d 1 /f
Write-Host '-- Disabling Optimizations for Windowed Games' -ForegroundColor Green
reg add "HKCU\Software\Microsoft\DirectX\UserGpuPreferences" /v "DirectXUserGlobalSettings" /t REG_SZ /d "SwapEffectUpgradeEnable=0;" /f
Write-Host '-- Disabling Game Bar' -ForegroundColor Green
reg add "HKLM\SOFTWARE\Polices\Microsoft\Windows\GameDVR" /v "AllowGameDVR" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\GameDVR" /v "AppCaptureEnabled" /t REG_DWORD /d 0 /f
reg add "HKCU\SOFTWARE\Microsoft\GameBar" /v "UseNexusForGameBarEnabled" /t REG_DWORD /d 0 /f
reg add "HKCU\SOFTWARE\Microsoft\GameBar" /v "ShowStartupPanel" /t REG_DWORD /d 0 /f
Write-Host '-- Disabling Xbox Screen Recording' -ForegroundColor Green
reg add "HKCU\System\GameConfigStore" /v "GameDVR_Enabled" /t REG_DWORD /d 0 /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\GameDVR" /v "AllowGameDVR" /t REG_DWORD /d 0 /f
Write-Host '-- Setting Cloudflare DNS' -ForegroundColor Green
netsh interface ip set dns name="Ethernet" static 1.1.1.1 | Out-Null
netsh interface ip add dns name="Ethernet" 1.0.0.1 index=2 | Out-Null
Write-Host '-- Set Ultimate Performance Power Plan' -ForegroundColor Green
$ultimatePerformance = powercfg -list | Select-String -Pattern 'Ultimate Performance'; if ($ultimatePerformance) { Write-Host '-- Power plan already exists.' } else { Write-Host 'Enabling Ultimate Performance.'; $output = powercfg -duplicatescheme e9a42b02-d5df-448d-aa00-03f14749eb61 2>&1; if ($output -match 'Unable to create a new power scheme' -or $output -match 'The power scheme, subgroup or setting specified does not exist') { powercfg -RestoreDefaultSchemes } }
$ultimatePlanGUID = (powercfg -list | Select-String -Pattern 'Ultimate Performance').Line.Split()[3]; Write-Host 'Activating Ultimate Performance'; powercfg -setactive $ultimatePlanGUID
Write-Host '-- Disabling Manual Services' -ForegroundColor Green
$manualServices = @("ALG","AppMgmt","AppReadiness","Appinfo","AxInstSV","BDESVC","BTAGService","BcastDVRUserService","BluetoothUserService","Browser","CDPSvc","COMSysApp","CaptureService","CertPropSvc","ConsentUxUserSvc","CscService","DevQueryBroker","DeviceAssociationService","DeviceInstall","DevicePickerUserSvc","DevicesFlowUserSvc","DisplayEnhancementService","DmEnrollmentSvc","DsSvc","DsmSvc","EFS","EapHost","EntAppSvc","FDResPub","FrameServer","FrameServerMonitor","GraphicsPerfSvc","HvHost","IEEtwCollectorService","InstallService","InventorySvc","IpxlatCfgSvc","KtmRm","LicenseManager","LxpSvc","MSDTC","MSiSCSI","McpManagementService","MicrosoftEdgeElevationService","MsKeyboardFilter","NPSMSvc","NaturalAuthentication","NcaSvc","NcbService","NcdAutoSetup","NetSetupSvc","Netman","NgcCtnrSvc","NgcSvc","NlaSvc","PNRPAutoReg","PcaSvc","PeerDistSvc","PenService","PerfHost","PhoneSvc","PimIndexMaintenanceSvc","PlugPlay","PolicyAgent","PrintNotify","PushToInstall","QWAVE","RasAuto","RasMan","RetailDemo","RmSvc","RpcLocator","SCPolicySvc","SCardSvr","SDRSVC","SEMgrSvc","SNMPTRAP","SNMPTrap","SSDPSRV","ScDeviceEnum","SensorDataService","SensorService","SensrSvc","SessionEnv","SharedAccess","SmsRouter","SstpSvc","StiSvc","StorSvc","TapiSrv","TextInputManagementService","TieringEngineService","TokenBroker","TroubleshootingSvc","TrustedInstaller","UdkUserSvc","UmRdpService","UserDataSvc","UsoSvc","VSS","VacSvc","WEPHOSTSVC","WFDSConMgrSvc","WMPNetworkSvc","WManSvc","WPDBusEnum","WalletService","WarpJITSvc","WbioSrvc","WdNisSvc","WdiServiceHost","WdiSystemHost","WebClient","Wecsvc","WerSvc","WiaRpc","WinRM","WpcMonSvc","WpnService","WwanSvc","autotimesvc","bthserv","camsvc","cbdhsvc","cloudidsvc","dcsvc","defragsvc","diagsvc","dmwappushservice","dot3svc","edgeupdate","edgeupdatem","embeddedmode","fdPHost","fhsvc","hidserv","icssvc","lfsvc","lltdsvc","lmhosts","msiserver","netprofm","p2pimsvc","p2psvc","perceptionsimulation","pla","seclogon","smphost","svsvc","swprv","upnphost","vds","vmicguestinterface","vmicheartbeat","vmickvpexchange","vmicrdv","vmicshutdown","vmictimesync","vmicvmsession","vmicvss","vmvss","wbengine","wcncsvc","webthreatdefsvc","wercplsupport","wisvc","wlidsvc","wlpasvc","wmiApSrv","workfolderssvc","wuauserv","wudfsvc")
$disabledServices = @("AppVClient","AssignedAccessManagerSvc","DiagTrack","DialogBlockingService","NetTcpPortSharing","RemoteAccess","RemoteRegistry","shpamsvc","ssh-agent","tzautoupdate")
$manualServices | ForEach-Object { Set-Service -Name $_ -StartupType Manual }
$disabledServices | ForEach-Object { Set-Service -Name $_ -StartupType Disabled }
Write-Host '-- Disabling Transparency' -ForegroundColor Green
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize" /v "EnableTransparency" /t REG_DWORD /d 0 /f
Write-Host '-- Disabling HAGS' -ForegroundColor Green
reg add "HKLM\SYSTEM\CurrentControlSet\Control\GraphicsDrivers" /v "HwSchMode" /t REG_DWORD /d 1 /f
Write-Host '-- Setting IPv4 Priority' -ForegroundColor Green
reg add "HKLM\SYSTEM\CurrentControlSet\Services\Tcpip6\Parameters" /v "DisabledComponents" /t REG_DWORD /d 32 /f
Write-Host '-- Disabling Mouse Delay Times' -ForegroundColor Green
reg add "HKCU\Control Panel\Desktop" /v "MenuShowDelay" /t REG_SZ /d 0 /f
reg add "HKCU\Control Panel\Mouse" /v "MouseHoverTime" /t REG_SZ /d 0 /f
Write-Host '-- Limiting Windows Defender Usage' -ForegroundColor Green
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows Defender\Scan" /v "AvgCPULoadFactor" /t REG_DWORD /d "25" /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows Defender\Scan" /v "ScanAvgCPULoadFactor" /t REG_DWORD /d "25" /f
Write-Host '-- Disabling Core Isolation' -ForegroundColor Green
reg add "HKLM\System\CurrentControlSet\Control\DeviceGuard\Scenarios\HypervisorEnforcedCodeIntegrity" /v "Enabled" /t REG_DWORD /d 0 /f
Write-Host '-- Disabling Storage Sense' -ForegroundColor Green
reg add "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\StorageSense\Parameters\StoragePolicy" /v "01" /t REG_DWORD /d 0 /f
Write-Host '-- Disabling Fast Startup' -ForegroundColor Green
reg add "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Power" /v "HiberbootEnabled" /t REG_DWORD /d 0 /f
Write-Host '-- Disabling Hibernation' -ForegroundColor Green
powercfg /h off
Write-Host '-- Enabling Dark Mode' -ForegroundColor Green
reg add "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Themes\Personalize" /v "AppsUseLightTheme" /t REG_DWORD /d 0 /f
reg add "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Themes\Personalize" /v "SystemUsesLightTheme" /t REG_DWORD /d 0 /f
Write-Host '-- Showing File Extensions' -ForegroundColor Green
reg add "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\Advanced" /v "HideFileExt" /t REG_DWORD /d 0 /f
Write-Host '-- Showing Hidden Files' -ForegroundColor Green
reg add "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\Advanced" /v "Hidden" /t REG_DWORD /d 1 /f
Write-Host '-- Disabling Sticky Keys' -ForegroundColor Green
reg add "HKCU\Control Panel\Accessibility\StickyKeys" /v "Flags" /t REG_SZ /d "58" /f
Write-Host '-- Disabling Snap Assist Flyout' -ForegroundColor Green
reg add "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\Advanced" /v "EnableSnapAssistFlyout" /t REG_DWORD /d 0 /f
Write-Host '-- Enabling Detailed BSOD' -ForegroundColor Green
reg add "HKLM\System\CurrentControlSet\Control\CrashControl" /v "DisplayParameters" /t REG_DWORD /d 1 /f
Write-Host '-- Enabling Verbose Logon' -ForegroundColor Green
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System" /v "VerboseStatus" /t REG_DWORD /d 1 /f
Write-Host '-- Disabling Multiplane Overlay (MPO)' -ForegroundColor Green
reg add "HKLM\SOFTWARE\Microsoft\Windows\Dwm" /v "OverlayTestMode" /t REG_DWORD /d "00000005" /f
Write-Host '-- Adding End Task to Right-Click' -ForegroundColor Green
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced\TaskbarDeveloperSettings" /v "TaskbarEndTask" /t REG_DWORD /d "1" /f
Write-Host '-- Removing Home and Gallery from File Explorer' -ForegroundColor Green
reg add "HKCU\Software\Classes\CLSID\{f874310e-b6b7-47dc-bc84-b9e6b38f5903}" /v "System.IsPinnedToNameSpaceTree" /t REG_DWORD /d "0" /f
reg add "HKCU\Software\Classes\CLSID\{e88865ea-0e1c-4e20-9aa6-edcd0212c87c}" /v "System.IsPinnedToNameSpaceTree" /t REG_DWORD /d "0" /f
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced" /v "LaunchTo" /t REG_DWORD /d "1" /f
Write-Host -- Installing these apps: 
Write-Host -- Brave.Brave, 7zip.7zip, AnyDesk.AnyDesk, CPUID.CPU-Z, CPUID.HWMonitor, Microsoft.VCRedist.2015+.x64, Skillbrains.Lightshot
$apps = @("Brave.Brave", "7zip.7zip", "AnyDesk.AnyDesk", "CPUID.CPU-Z", "CPUID.HWMonitor", "Microsoft.VCRedist.2015+.x64", "Skillbrains.Lightshot"); foreach ($app in $apps) { winget install $app --accept-source-agreements --accept-package-agreements --force }
Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
Start-Process explorer.exe
KC-Finish
KC-SelfClean
Pause
exit
