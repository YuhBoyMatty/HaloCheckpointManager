﻿using System;
using System.IO;
using System.Diagnostics;
using HCMExternal.Models;
using Serilog;

namespace HCMExternal.Services.CheckpointServiceNS
{


    public partial class CheckpointService
    {
        /// <summary>
        /// Prompts the user to rename a saveFolder (this will change the actual folder name).
        /// </summary>
        /// <param name="SelectedSaveFolder">The saveFolder to rename.</param>
        /// <exception cref="Exception"></exception>
        /// <exception cref="InvalidOperationException"></exception>
        public void RenameFolder(SaveFolder? SelectedSaveFolder)
        {
            if (SelectedSaveFolder == null) throw new Exception("Can't rename - no savefolder selected");


            // Ask user what they want to name the folder
            string? userInput = Microsoft.VisualBasic.Interaction.InputBox(@"Must be unique, no fancy characters",
                                                       $"Rename folder: {SelectedSaveFolder.SaveFolderName}",
                                                       $"{SelectedSaveFolder.SaveFolderName}",
                                                       -1, -1);

            if (userInput == null) return; //They clicked the cancel button

            string proposedFolder = SelectedSaveFolder.ParentPath + "\\" + userInput;
            Log.Verbose("proposed folder: " + proposedFolder);
            // Some basic but not comprehensive checks that the user inputted a valid value (trycatch will find the rest of invalids)
            if (userInput == "" || Directory.Exists(proposedFolder)) throw new InvalidOperationException("Failed to rename savefolder; was your new name valid and unique?");


                Directory.Move(SelectedSaveFolder.SaveFolderPath, proposedFolder);




        }
    }
   
}
