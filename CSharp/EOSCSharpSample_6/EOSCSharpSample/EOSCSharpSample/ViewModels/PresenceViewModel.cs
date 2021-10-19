// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.Commands;
using EOSCSharpSample.Helpers;
using Epic.OnlineServices.Presence;
using System;
using System.Collections.Generic;
using System.Linq;

namespace EOSCSharpSample.ViewModels
{
    public class PresenceViewModel : BindableBase
    {
        private Status _status;
        public Status Status
        {
            get { return _status; }
            set { SetProperty(ref _status, value); }
        }

        private string _productIdText;
        public string ProductIdText
        {
            get { return _productIdText; }
            set { SetProperty(ref _productIdText, value); }
        }

        private string _productVersionText;
        public string ProductVersionText
        {
            get { return _productVersionText; }
            set { SetProperty(ref _productVersionText, value); }
        }

        private string _platformText;
        public string PlatformText
        {
            get { return _platformText; }
            set { SetProperty(ref _platformText, value); }
        }

        private string _richText;
        public string RichText
        {
            get { return _richText; }
            set { SetProperty(ref _richText, value); }
        }

        private List<Enum> _statusOptions;
        public List<Enum> StatusOptions
        {
            get { return _statusOptions; }
            set { SetProperty(ref _statusOptions, value); }
        }

        public PresenceQueryCommand PresenceQuery { get; set; }
        public PresenceModifyStatusCommand PresenceModifyStatus { get; set; }

        public PresenceViewModel()
        {
            StatusOptions = Enum.GetValues(Status.GetType()).Cast<Enum>().ToList();

            PresenceQuery = new PresenceQueryCommand();
            PresenceModifyStatus = new PresenceModifyStatusCommand();
        }

    }
}
