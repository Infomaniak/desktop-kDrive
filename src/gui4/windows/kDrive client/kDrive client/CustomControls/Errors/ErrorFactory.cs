using Infomaniak.kDrive.CustomControls.Errors.Templates;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Linq;
using System.Reflection;
using WinRT;

namespace Infomaniak.kDrive.CustomControls.Errors
{
    internal class ErrorFactory
    {
        private static readonly Type UserControlType = typeof(UserControl);
        private static (Type type, ErrorMetadataAttribute meta)[]? _cache;

        public static UserControl CreateErrorControl(Error error)
        {
            var candidates = GetCandidates();
            if (candidates == null)
            {
                Logger.Log(Logger.Level.Error, "No candidates found for error control creation.");
                return new DefaultError(error);
            }

            // Try to find a perfect match
            var perfectMatch = candidates
                .Select(c => (c.type, c.meta)).Where(c =>
                c.meta.ExitCodes.Contains(error.ExitCode) &&
                c.meta.ExitCauses.Contains(error.ExitCause) &&
                c.meta.Levels.Contains(error.ErrorLevel) &&
                c.meta.NodeTypes.Contains(error.NodeType) &&
                c.meta.ConflictTypes.Contains(error.ConflictType) &&
                c.meta.InconsistencyTypes.Contains(error.InconsistencyType) &&
                c.meta.CancelTypes.Contains(error.CancelType));

            if (!perfectMatch.Any())
            {
                Logger.Log(Logger.Level.Warning, $"No perfect match found for error control creation (error: {error}). Using default error control.");
                return new DefaultError(error);
            }
            else if (perfectMatch.Count() > 1)
            {
                Logger.Log(Logger.Level.Error, $"Multiple perfect matches found for error control creation (error: {error}). Using the first match ${perfectMatch.First().type.FullName}.");
            }

            var matchType = perfectMatch.First().type;
            return (UserControl)Activator.CreateInstance(matchType, error)!;
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
