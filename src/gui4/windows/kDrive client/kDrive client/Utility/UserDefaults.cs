/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
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
