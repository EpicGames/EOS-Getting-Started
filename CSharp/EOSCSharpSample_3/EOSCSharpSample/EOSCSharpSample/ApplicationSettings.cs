// Copyright Epic Games, Inc. All Rights Reserved.

using Epic.OnlineServices.Platform;
using System.Collections.Generic;

namespace EOSCSharpSample
{
    public class ApplicationSettings
    {
        // Go to https://dev.epicgames.com/portal/ to get the following values
        // Select your product and click Product Settings in the menu
        // Product -> Product ID
        public string ProductId { get; private set; } = "";
        // Clients -> Client ID and Client Secret
        public string ClientId { get; private set; } = "";
        public string ClientSecret { get; private set; } = "";
        // Sandboxes -> Sandbox ID
        public string SandboxId { get; private set; } = "";
        // Deployments -> Deployment ID
        public string DeploymentId { get; private set; } = "";

        public PlatformInterface PlatformInterface { get; set; }

        public void Initialize(Dictionary<string, string> commandLineArgs)
        {
            // Use command line arguments if passed
            ProductId = commandLineArgs.ContainsKey("-productid") ? commandLineArgs.GetValueOrDefault("-productid") : ProductId;
            SandboxId = commandLineArgs.ContainsKey("-sandboxid") ? commandLineArgs.GetValueOrDefault("-sandboxid") : SandboxId;
            DeploymentId = commandLineArgs.ContainsKey("-deploymentid") ? commandLineArgs.GetValueOrDefault("-deploymentid") : DeploymentId;
            ClientId = commandLineArgs.ContainsKey("-clientid") ? commandLineArgs.GetValueOrDefault("-clientid") : ClientId;
            ClientSecret = commandLineArgs.ContainsKey("-clientsecret") ? commandLineArgs.GetValueOrDefault("-clientsecret") : ClientSecret;
        }
    }
}
