﻿

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Diagnostics;
using BurntMemory;
using HCM3.View;
using System.Collections.ObjectModel;
using HCM3.ViewModel;
using HCM3.Model;


namespace HCM3.View
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        #region Data
        #endregion // Data


        #region Constructor
        public MainWindow()
        {
            
       HCMSetup setup = new();


            // Run some checks; have admin priviledges, have file access, have required folders & files.
            if (!setup.HCMSetupChecks(out string errorMessage))
            {
                // If a check fails, tell the user why, then shutdown the application.
                System.Windows.MessageBox.Show(errorMessage, "Error", System.Windows.MessageBoxButton.OK);
                System.Windows.Application.Current.Shutdown();
            }

            // Create collection of all our ReadWrite.Pointers and load them from the online repository
            PointerCollection pcollection = new();
            if (!pcollection.LoadPointersFromGit(out string error))
            {
                System.Windows.MessageBox.Show(error, "Error", System.Windows.MessageBoxButton.OK);
                System.Windows.Application.Current.Shutdown();
            }

            HCMTasks tasks = new();
            HaloMemory HaloMemory = new HaloMemory();
            InitializeComponent();
            ObservableCollection<Checkpoint> checkpointsH1 = new();

            ObservableCollection<Checkpoint> checkpointsH2 = new();
            ObservableCollection<Checkpoint> checkpointsH3 = new();
            ObservableCollection<Checkpoint> checkpointsH3ODST = new();
            ObservableCollection<Checkpoint> checkpointsHReach = new();
            ObservableCollection<Checkpoint> checkpointsH4 = new();

            Halo1Model H1model = new Halo1Model(HaloMemory, checkpointsH1);
            Halo1ViewModel H1viewModel = new Halo1ViewModel(H1model, checkpointsH1);
            
            checkpointsH1.Add(new Checkpoint("c1"));
            checkpointsH1.Add(new Checkpoint("c2"));
            checkpointsH1.Add(new Checkpoint("c3"));
            Checkpoint test = new Checkpoint("c4");
            checkpointsH1.Add(test);
            this.DataContext = H1viewModel;
           
            test.CheckpointName = "grrr";
            

            //checkpointsH1.Add(new Checkpoint());

        }
        #endregion // Constructor

        #region Properties

        #endregion
        private void Window_Closed(object sender, EventArgs e)
        {
            Settings.Default.Save();
        }



    }

   



    //yoink https://stackoverflow.com/a/61475351
    public static class GridColumn
    {
        public static readonly DependencyProperty MinWidthProperty =
            DependencyProperty.RegisterAttached("MinWidth", typeof(double), typeof(GridColumn), new PropertyMetadata(75d, (s, e) => {
                if (s is GridViewColumn gridColumn)
                {
                    SetMinWidth(gridColumn);
                    ((System.ComponentModel.INotifyPropertyChanged)gridColumn).PropertyChanged += (cs, ce) => {
                        if (ce.PropertyName == nameof(GridViewColumn.ActualWidth))
                        {
                            SetMinWidth(gridColumn);
                        }
                    };
                }
            }));

        private static void SetMinWidth(GridViewColumn column)
        {
            double minWidth = (double)column.GetValue(MinWidthProperty);

            if (column.Width < minWidth)
                column.Width = minWidth;
        }

        public static double GetMinWidth(DependencyObject obj) => (double)obj.GetValue(MinWidthProperty);

        public static void SetMinWidth(DependencyObject obj, double value) => obj.SetValue(MinWidthProperty, value);
    }

}
