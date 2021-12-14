// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.ComponentModel;
using System.Globalization;
using System.Windows.Data;

namespace EOSCSharpSample.Converters
{
    [Bindable(BindableSupport.Default)]
    public class StringToBooleanConverter : IValueConverter
    {

        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value == null || !(value is string))
            {
                return false;
            }

            string stringValue = value as string;

            return !string.IsNullOrWhiteSpace(stringValue.Trim());
        }
        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }
}
