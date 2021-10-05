// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.ComponentModel;
using System.Globalization;
using System.Windows;
using System.Windows.Data;

namespace EOSCSharpSample.Converters
{
    [Bindable(BindableSupport.Default)]
    public class StringToVisibilityConverter : IValueConverter
    {

        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value == null || !(value is string))
            {
                return Visibility.Collapsed;
            }

            string stringValue = value as string;

            return string.IsNullOrWhiteSpace(stringValue.Trim()) ? Visibility.Collapsed : (object)Visibility.Visible;
        }
        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }
}
