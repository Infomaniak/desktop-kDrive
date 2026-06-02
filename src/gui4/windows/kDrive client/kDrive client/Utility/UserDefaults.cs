using System;
using System.IO;
using System.Text.Json.Nodes;

namespace Infomaniak.kDrive
{
    internal class UserDefaults
    {
        private readonly string userDefaultsFilePath = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "kDrive",
                ".userprefs");

        private readonly JsonObject userDefaults;


        public UserDefaults()
        {
            if (File.Exists(userDefaultsFilePath))
            {
                try
                {
                    string jsonContent = File.ReadAllText(userDefaultsFilePath);
                    userDefaults = JsonNode.Parse(jsonContent)?.AsObject() ?? [];
                }
                catch (Exception)
                {
                    userDefaults = [];
                }
            }
            else
            {
                userDefaults = [];
            }
        }

        public void SetValue(string key, JsonNode value)
        {
            userDefaults[key] = value;
            Save();
        }

        public JsonNode? GetValue(string key)
        {
            if (userDefaults.TryGetPropertyValue(key, out JsonNode? value))
            {
                return value;
            }
            return null;
        }

        private void Save()
        {
            try
            {
                string jsonContent = userDefaults.ToJsonString();
                Directory.CreateDirectory(Path.GetDirectoryName(userDefaultsFilePath) ?? string.Empty);
                File.WriteAllText(userDefaultsFilePath, jsonContent);
            }
            catch (Exception)
            {
                Logger.Log(Logger.Level.Warning, "Failed to save user defaults.");
            }
        }
    }
}
