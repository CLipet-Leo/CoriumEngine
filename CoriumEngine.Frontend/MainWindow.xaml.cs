using System;
using System.Windows;

namespace CoriumEngine.Frontend
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
        }

        protected override void OnClosed(EventArgs e)
        {
            DxHost?.Dispose();
            base.OnClosed(e);
        }
    }
}
