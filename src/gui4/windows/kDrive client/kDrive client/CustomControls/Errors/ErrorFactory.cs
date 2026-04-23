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
﻿using DynamicData;
using Infomaniak.kDrive.CustomControls.Errors.Templates;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;

namespace Infomaniak.kDrive.CustomControls.Errors
{
    internal class ErrorFactory
    {
        private static readonly Type UserControlType = typeof(UserControl);
        public sealed record ErrorCardInfos(Type Type, ErrorMetadataAttribute Meta);
        private static (Type type, ErrorMetadataAttribute meta)[]? _cache;

        private static readonly Dictionary<DbId, ErrorCardInfos> _cacheByDbId = new();
        public static UserControl CreateErrorControl(Error error)
        {
            var matchType = GetErrorCardInfos(error)?.Type;
            if (matchType is null)
            {
                Logger.Log(Logger.Level.Warning, $"No matching control type found for error: {error}. Returning default error control.");
                return new DefaultError(error);
            }

            return (UserControl)Activator.CreateInstance(matchType, error)!;
        }

        private static ErrorCardInfos? GetErrorCardInfosFromCache(Error error)
        {
            if (_cacheByDbId.TryGetValue(error.DbId, out var entry))
            {
                // Verify that the cached entry still matches the error properties
                if (entry.Meta.matches(error))
                {
                    Logger.Log(Logger.Level.Extended, $"Cache hit for error DbId {error.DbId}. Returning cached entry: {entry}");
                    return entry;
                }
                else
                {
                    Logger.Log(Logger.Level.Info, $"Cached entry for error DbId {error.DbId} does not match current error properties. Cache miss.");
                    _cacheByDbId.Remove(error.DbId);
                }
            }

            return null;
        }

        public static ErrorCardInfos? GetErrorCardInfos(Error error)
        {
            var result = GetErrorCardInfosFromCache(error);

            if (result is not null)
                return result;

            // Cache miss, need to find the best control type in the assembly
            var candidates = GetCandidates();
            if (candidates == null)
            {
                Logger.Log(Logger.Level.Error, "No candidates found for best control type retrieval.");
                return null;
            }

            // Try to find a perfect match
            var perfectMatch = candidates.Select(c => (c.type, c.meta)).Where(c => c.meta.matches(error));

            if (!perfectMatch.Any())
            {
                Logger.Log(Logger.Level.Warning, $"No perfect match found for best control type retrieval (error: {error}).");
                return null;
            }
            else if (perfectMatch.Count() > 1)
            {
                Logger.Log(Logger.Level.Error, $"Multiple perfect matches found for best control type retrieval (error: {error}). Returning the first match ${perfectMatch.First().type.FullName}.");
            }
            
            result = new ErrorCardInfos(perfectMatch.First().type, perfectMatch.First().meta);
            _cacheByDbId.Add(error.DbId, result);
            return result;
        }


        private static (Type type, ErrorMetadataAttribute meta)[] GetCandidates()
        {
            if (_cache != null)
            {
                return _cache;
            }

            var asm = typeof(ErrorFactory).Assembly;
            _cache = asm.GetTypes()
                .Where(t =>
                    UserControlType.IsAssignableFrom(t) &&
                    t.GetCustomAttribute<ErrorMetadataAttribute>(inherit: false) != null &&
                    t.GetConstructors(BindingFlags.Public | BindingFlags.Instance)
                     .Any(ctr =>
                         ctr.GetParameters().Length == 1 &&
                         ctr.GetParameters()[0].ParameterType == typeof(Error)))
                .Select(t => (t, t.GetCustomAttribute<ErrorMetadataAttribute>(false)!))
                .ToArray();

            foreach (var candidate in _cache)
            {
                Logger.Log(Logger.Level.Extended, $"Registered error control candidate: {candidate.type.FullName} with metadata: {candidate.meta}");
            }
            return _cache;
        }
    }
}
