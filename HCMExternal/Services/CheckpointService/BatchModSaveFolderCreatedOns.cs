﻿using System;
using System.Collections.Generic;
using HCMExternal.Models;
using System.IO;
using System.Diagnostics;
using Serilog;

namespace HCMExternal.Services.CheckpointServiceNS
{
    public partial class CheckpointService
    {
        /// <summary>
        /// Rewrites the createdOnTimes dates of a list of save folders according to a specified order. CreatedOnTimes are used to order saveFolders with a shared parent.
        /// </summary>
        /// <param name="listSaveFolders">List (in order) of saveFolders (with a shared parent) whose createdOnTimes will be modified.</param>
        /// <param name="listCreatedOns">List (in order) of the new createdOnTimes.</param>
        public void BatchModSaveFolderCreatedOns(List<SaveFolder> listSaveFolders, List<DateTime?> listCreatedOns)
        {
            for (int i = 0; i < listSaveFolders.Count; i++)
            {
                string saveFolderPath = listSaveFolders[i].SaveFolderPath;
                DirectoryInfo saveFolderInfo = new(saveFolderPath);
                if (saveFolderInfo.Exists && listCreatedOns[i] != null)
                {
                    saveFolderInfo.CreationTime = listCreatedOns[i].Value;
                }
                else
                {
                    Log.Error("Something went wrong modifying SaveFolderCreatedOns; path: " + saveFolderPath);
                }
            }
        }
    }
}
