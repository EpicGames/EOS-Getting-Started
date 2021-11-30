// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Windows.Input;

namespace EOSCSharpSample.Helpers
{
    public class CommandBase : ICommand
    {
        public virtual bool CanExecute(object parameter)
        {
            throw new NotImplementedException();
        }

        public virtual void Execute(object parameter)
        {
            throw new NotImplementedException();
        }

        public event EventHandler CanExecuteChanged;
        public void RaiseCanExecuteChanged()
        {
            CanExecuteChanged?.Invoke(this, new EventArgs());
        }
    }
}
