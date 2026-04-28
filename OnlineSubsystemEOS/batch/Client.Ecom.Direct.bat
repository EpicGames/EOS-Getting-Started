@echo off
REM Tutorial 10: Launch a packaged client DIRECTLY against the EcomClient artifact
REM (different Dev Portal product configured for Game Catalog). Bypasses the EAC
REM bootstrapper since ecom flows don't require AC protection - faster iteration
REM during ecom development.
REM
REM Why a separate artifact: the project's main Client / Server / P2PClient
REM artifacts point at a Dev Portal product without a Game Catalog, so
REM IOnlineStoreV2::QueryOffersByFilter returns 0 results. EcomClient is a
REM different product whose Dev Portal entry has Catalog Items + Offers
REM configured, so the ecom queries actually return data.
REM
REM Checkout flow REQUIRES the EOS overlay to work (bEnableOverlay=True is
REM already set in DefaultEngine.ini). The EOS overlay loads in any standalone
REM packaged build - the EAC bootstrapper is unrelated.
REM
REM Copy this file to batch/local/Client.Ecom.Direct.bat and edit paths +
REM DevAuth credential. To exercise the checkout path, type
REM `TestCheckoutFirstOffer` in the in-game console (`~`).

cd /d "<Path to packaged Client Binaries\Win64 dir>"

EOS_OSS_Tutorial.exe ^
    ThirdPersonMap ^
    -debug -game ^
    -AUTH_TYPE="developer" ^
    -AUTH_LOGIN="localhost:8081" ^
    -AUTH_PASSWORD="<DevAuthTool Credential name>" ^
    -epicapp="EcomClient"
