// Copyright Epic Games, Inc. All Rights Reserved.

using Epic.OnlineServices;
using Epic.OnlineServices.Auth;
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

        // In this sample we use the AccountPortal login type, which prompts the user to log in and consent in a browser
        // For other login types, please refer to https://dev.epicgames.com/docs/services/en-US/API/Members/Enums/Auth/EOS_ELoginCredentialType/index.html
        public LoginCredentialType LoginCredentialType { get; private set; } = LoginCredentialType.AccountPortal;
        /// The use of Id and Token differs based on the login type.
        public string Id { get; private set; } = "";
        public string Token { get; private set; } = "";

        // In this sample we use Epic account login, but note that you are also able to use other providers
        // Please refer to https://dev.epicgames.com/docs/services/en-US/API/Members/Enums/NoInterface/EOS_EExternalCredentialType/index.html
        public ExternalCredentialType ExternalCredentialType { get; private set; } = ExternalCredentialType.Epic;

        // Define the scopes you want permission to access for the logged in user
        // Please refer to https://dev.epicgames.com/docs/services/en-US/API/Members/Enums/Auth/EOS_EAuthScopeFlags/index.html
        public AuthScopeFlags ScopeFlags
        {
            get
            {
                return AuthScopeFlags.BasicProfile | AuthScopeFlags.Presence;
            }
        }

        public void Initialize(Dictionary<string, string> commandLineArgs)
        {
            // Use command line arguments if passed
            ProductId = commandLineArgs.ContainsKey("-productid") ? commandLineArgs.GetValueOrDefault("-productid") : ProductId;
            SandboxId = commandLineArgs.ContainsKey("-sandboxid") ? commandLineArgs.GetValueOrDefault("-sandboxid") : SandboxId;
            DeploymentId = commandLineArgs.ContainsKey("-deploymentid") ? commandLineArgs.GetValueOrDefault("-deploymentid") : DeploymentId;
            ClientId = commandLineArgs.ContainsKey("-clientid") ? commandLineArgs.GetValueOrDefault("-clientid") : ClientId;
            ClientSecret = commandLineArgs.ContainsKey("-clientsecret") ? commandLineArgs.GetValueOrDefault("-clientsecret") : ClientSecret;

            LoginCredentialType = commandLineArgs.ContainsKey("-logincredentialtype") ? (LoginCredentialType)System.Enum.Parse(typeof(LoginCredentialType), commandLineArgs.GetValueOrDefault("-logincredentialtype")) : LoginCredentialType;
            Id = commandLineArgs.ContainsKey("-id") ? commandLineArgs.GetValueOrDefault("-id") : Id;
            Token = commandLineArgs.ContainsKey("-token") ? commandLineArgs.GetValueOrDefault("-token") : Token;
            ExternalCredentialType = commandLineArgs.ContainsKey("-externalcredentialtype") ? (ExternalCredentialType)System.Enum.Parse(typeof(ExternalCredentialType), commandLineArgs.GetValueOrDefault("-externalcredentialtype")) : ExternalCredentialType;
        }
    }
}
