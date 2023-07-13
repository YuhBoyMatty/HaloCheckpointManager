﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO;
using System.Diagnostics;
using System.Windows;
using HCMExternal.Models;

namespace HCMExternal.Services.CheckpointServiceNS
{

    public partial class CheckpointService
    {

        /// <summary>
        /// Deletes a saveFolder containing checkpoints.
        /// </summary>
        /// <param name="SelectedSaveFolder">The saveFolder to delete.</param>
        /// <exception cref="Exception"></exception>
        public void DeleteFolder(SaveFolder? SelectedSaveFolder)
        {
            if (SelectedSaveFolder == null) throw new Exception("Can't delete - no savefolder selected");

            //We want to count how many checkpoints are in this folder and sub-folders so we can warn the user that they may be about to delete many checkpoints.

            int checkpointCount = Directory.GetFiles(SelectedSaveFolder.SaveFolderPath, "*.bin", SearchOption.AllDirectories).Length;


            if (MessageBox.Show("Are you sure you want to delete the folder \"" + SelectedSaveFolder.SaveFolderName + "\", and all it's subfolders?"
                + "\n" + "This will delete " + checkpointCount + " checkpoint(s) contained within, as well as any other files."
                , "HCM - Delete folder?", MessageBoxButton.YesNo, MessageBoxImage.Warning) == MessageBoxResult.Yes)
            {
               Directory.Delete(SelectedSaveFolder.SaveFolderPath, true);
            }

                




        }
    }
   
}
