﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Diagnostics;
using System.IO;
using System.Xml.Linq;
using BurntMemory;
using HCM3.Helpers;
using HCM3.Services.Trainer;

namespace HCM3.Services
{
    public partial class DataPointersService
    {

        public void LoadPointerDataFromSource2(out string failedReads)
        {
            string? xml = null;
            string localPointerDataPath = Directory.GetCurrentDirectory() + "\\PointerData.xml";
            failedReads = "";

            // Download the xml from git
            try
            {
                throw new Exception("for debugging we'll use local file");
                string url = "https://raw.githubusercontent.com/Burnt-o/HaloCheckpointManager/HCM2/HCM3/PointerData.xml";
                System.Net.WebClient client = new System.Net.WebClient();
                xml = client.DownloadString(url);

                //Write the contents to local PointerData.xml for offline use
                if (File.Exists(localPointerDataPath)) File.Delete(localPointerDataPath);
                File.WriteAllText(localPointerDataPath, xml);
            }
            catch
            {
                // Couldn't grab online data, try offline backup
                Trace.WriteLine("Couldn't find online xml data, trying local backup");
                if (File.Exists(@"C:\Users\mauri\source\repos\HaloCheckpointManager\HCM3\PointerData.xml"))
                {
                    Trace.WriteLine("grabbing debug xml from repo");
                    xml = File.ReadAllText(@"C:\Users\mauri\source\repos\HaloCheckpointManager\HCM3\PointerData.xml");
                }
                else if (File.Exists(localPointerDataPath))
                {
                    Trace.WriteLine("grabbing local xml data");
                    xml = File.ReadAllText(localPointerDataPath);
                }
            }

            if (xml == null || !xml.Any()) throw new Exception("Couldn't find pointerdata!");

            // Deserialise
            XDocument doc = XDocument.Parse(xml);

            if (doc.Root == null) throw new Exception("Something went wrong parsing the Pointer data xml file; 'doc.Root' was null.");

            foreach (XElement entry in doc.Root.Elements())
            {
                try
                {
                    object? entryObject = null;
                    string? entryName = null;
                    string? entryVersion = null;

                    if (entry.Name == "HighestSupportedVersion")
                    {
                        this.HighestSupportedMCCVersion = entry.Value == null || entry.Value == "" ? null : entry.Value;
                        Trace.WriteLine("Loaded HighestSupportedMCCVersion, value: " + this.HighestSupportedMCCVersion);
                    }
                    else if (entry.Name == "Entry")
                    {
                        string entryType = entry.Attribute("Type").Value;
                        entryName = entry.Element("Name") == null ? null : entry.Element("Name")?.Value;
                        foreach (XElement VersionEntry in entry.Elements().Where(x => x.Name == "Version"))
                        {
                            entryVersion = VersionEntry.Attribute("Version").Value;
                            entryObject = ParseObject(VersionEntry, entryType);
                            StoreObject(entryName, entryVersion, entryObject);
                        }
                    }

                }
                catch (Exception ex)
                { 
                    failedReads = failedReads + "\n" + "Error processing an entry in pointerdata, " + ex.Message + ", entry.name: " + entry.Element("Name")?.Value + ", entry.Value: " + entry.Value;
                Trace.WriteLine("Error processing an entry in pointerdata, " + ex.Message);
                    continue;
                }

            }


        }
        public object ParseObject(XElement entry, string type)
        {
            object? returnObject = null;
            switch (type)
            {
                case "int":
                    returnObject = ParseHexNumber(entry.Element("Offset")?.Value);
                    break;

                case "PreserveLocation[]":
                    PreserveLocation[] preserveLocationArray = entry.Element("Array")?.Elements().Select(x => ParseLocation(x)).ToArray();
                    if (!preserveLocationArray.Any()) throw new Exception("Emptry PreserveLocation array");
                    returnObject = preserveLocationArray;
                    break;

                case "ReadWrite.Pointer":
                    returnObject = ParsePointer(entry);
                    break;

                case "ReadWrite.Pointer[]":
                    ReadWrite.Pointer?[]? pointerArray = entry.Element("Array")?.Elements().Select(x => ParsePointer(x)).ToArray();
                    if (!pointerArray.Any()) throw new Exception("Emptry Pointer Array");
                    returnObject = pointerArray;
                    break;

                case "int[]":
                    int[] intArray = entry.Element("Array")?.Elements().Select(x => ParseHexNumber(x.Value)).ToArray();
                    if (!intArray.Any()) throw new Exception("Emptry intArray");
                    returnObject = intArray;
                    break;

                case "byte[]":
                    byte[] byteArray = StringToByteArray(entry.Element("Array")?.Value);
                    if (!byteArray.Any()) throw new Exception("Emptry byteArray");
                    returnObject = byteArray;
                    break;

                case "string":
                    returnObject = entry.Element("Value")?.Value;
                    break;

                case "DetourInfoObject":
                    returnObject = ParseDetourInfoObject(entry);
                    break;

                case "Dictionary<string, bool>":
                    Dictionary<string,bool> entryDictionary = new();
                    foreach (XElement element in entry.Elements())
                    {
                        entryDictionary.Add(element.Name.ToString(), Convert.ToBoolean(element.Value));
                    }
                    returnObject = entryDictionary;
                break;


                default:
                    throw new ArgumentException("Didn't recognise type of datapointer entry, " + type);
            }
            if (returnObject == null) throw new Exception("Failed to parse returnObject but not sure why");
            return returnObject;
        }

        public void StoreObject(string entryName, string entryVersion, object entryObject)
        {
            if (entryObject == null) throw new Exception("entryObject was somehow null at StoreObject");
            if (entryObject.GetType() == typeof(System.Xml.Linq.XElement)) throw new Exception("Accidentally parsed XElement");

            Dictionary<string, Object> versionDictionary = new();
            versionDictionary.Add(entryVersion, entryObject);
            PointerData.Add(entryName, versionDictionary);
            Trace.WriteLine("Added new object to pointer dictionary, name: " + entryName + ", version: " + entryVersion + ", type: " + entryObject.GetType().ToString());
        }






        PreserveLocation? ParseLocation(XElement? location)
        {
            if (location == null) return null;
            //Trace.WriteLine("location: " + location.ToString());
            try
            {

                int? Offset = ParseHexNumber(location.Element("Offset")?.Value);
                int? Length = ParseHexNumber(location.Element("Length")?.Value);

                if (Offset == null || Length == null)
                {
                    Trace.WriteLine("offset or length was null when parsing location");
                    return null;
                }

                return new PreserveLocation((int)Offset, (int)Length);
            }
            catch
            {
                return null;
            }


        }


        int ParseHexNumber(string? s)
        {
            if (s == null)
                throw new Exception("int was null");
            return s.StartsWith("0x") ? Convert.ToInt32(s.Substring(2), 16) : Convert.ToInt32(s);
        }


        ReadWrite.Pointer ParsePointer(XElement entry)
        {
            string? pointerModule = entry.Element("Module") == null ? null : entry.Element("Module")?.Value;
            int[]? pointerOffsets = entry.Element("Offsets") == null ? null : entry.Element("Offsets")?.Elements().Select(x => ParseHexNumber(x.Value)).ToArray();

            if (pointerModule == null) throw new Exception("ReadWrite.Pointer: pointerModule was null, name: " + entry.Element("Name")?.Value);
            if (pointerOffsets == null) throw new Exception("ReadWrite.Pointer: pointerOffsets was null, name: " + entry.Element("Name")?.Value);

            return new ReadWrite.Pointer(pointerModule, pointerOffsets);
        }

        DetourInfoObject ParseDetourInfoObject(XElement entry)
        {
            ReadWrite.Pointer OriginalCodeLocation = ParsePointer(entry.Element("OriginalCodeLocation"));
            int SizeToAlloc = ParseHexNumber(entry.Element("SizeToAlloc")?.Value);
            string DetourCodeASM = entry.Element("DetourCodeASM")?.Value ?? throw new Exception("failed reading entry DetourCodeASM");
            string HookCodeASM = entry.Element("HookCodeASM")?.Value ?? throw new Exception("failed reading entry HookCodeASM");

            byte[] OriginalCodeBytes = entry.Element("OriginalCodeBytes") == null ? throw new Exception("failed reading entry OriginalCodeBytes") : StringToByteArray(entry.Element("OriginalCodeBytes")?.Value);
            if (!OriginalCodeBytes.Any()) throw new Exception("failed reading entry for detourinfoobject, null values in OriginalCodeBytes");

            // Parse symbols in symbolPointers xml
            Dictionary<string, ReadWrite.Pointer> SymbolPointers = new();

            if (entry.Element("SymbolPointers") != null && entry.Element("SymbolPointers").HasElements)
            {
                foreach (XElement Pointer in entry.Element("SymbolPointers").Elements())
                {

                    ReadWrite.Pointer? newPointer = ParsePointer(Pointer);
                    string? symbolName = Pointer.Element("Symbol")?.Value ?? throw new Exception("failed reading symbol for symbolPointer");
                    if (newPointer == null || symbolName == null) throw new Exception("failed reading symbolpointer");
                    SymbolPointers.Add(symbolName, newPointer);

                }
            }
            // Parse automatic symbol $returnControl
            // returnControl is OriginalCodeLocation + the number of bytes in Original Code bytes
            SymbolPointers.Add("$returnControl", OriginalCodeLocation + OriginalCodeBytes.Length);
            return new DetourInfoObject(OriginalCodeLocation, OriginalCodeBytes, SizeToAlloc, DetourCodeASM, HookCodeASM, SymbolPointers);

        }

        string ByteArrayToString(byte[] byteArray)
        {
            string ByteArrayToString = "";
            for (int i = 0; i < byteArray.Length; i++)
            {
                ByteArrayToString = ByteArrayToString + "0x" + byteArray[i].ToString("X2") + ",";
            }

            Trace.WriteLine(ByteArrayToString);
            ByteArrayToString = ByteArrayToString.Remove(ByteArrayToString.Length - 1);
            Trace.WriteLine(ByteArrayToString);
            return ByteArrayToString;
        }

        byte[] StringToByteArray(string str)
        {

            str = str.Replace("0x", "");
            str = str.Replace(" ", "");
            string[] splitstr = str.Split(',');
            byte[] byteArray = new byte[splitstr.Length];
            for (int i = 0; i < splitstr.Length; i++)
            {
                byteArray[i] = Convert.ToByte(splitstr[i], 16);
            }
            return byteArray;
        }


    }
}