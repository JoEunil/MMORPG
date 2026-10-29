using System;
using System.IO;
using System.Runtime.ExceptionServices;
using System.Text;

namespace Mmorpg.Client.Editor
{
    internal static class ExportPair
    {
        private const string PendingMarker = "C14Export.pending";
        private static readonly UTF8Encoding Utf8 = new UTF8Encoding(false);

        public static void Write(string outputDirectory, string firstName, string firstText, string secondName, string secondText)
        {
            ValidateName(firstName);
            ValidateName(secondName);
            if (string.Equals(firstName, secondName, StringComparison.OrdinalIgnoreCase) ||
                firstName == PendingMarker || secondName == PendingMarker)
                throw new ArgumentException("Export pair names must be distinct and cannot use the pending marker.");

            Directory.CreateDirectory(outputDirectory);
            string firstPath = Path.Combine(outputDirectory, firstName);
            string secondPath = Path.Combine(outputDirectory, secondName);
            RejectReadOnly(firstPath);
            RejectReadOnly(secondPath);

            string markerPath = Path.Combine(outputDirectory, PendingMarker);
            string id = Guid.NewGuid().ToString("N");
            string firstStage = Path.Combine(outputDirectory, "." + firstName + "." + id + ".stage");
            string secondStage = Path.Combine(outputDirectory, "." + secondName + "." + id + ".stage");
            string firstBackup = Path.Combine(outputDirectory, "." + firstName + "." + id + ".backup");
            string secondBackup = Path.Combine(outputDirectory, "." + secondName + "." + id + ".backup");
            bool firstExisted = File.Exists(firstPath);
            bool secondExisted = File.Exists(secondPath);
            CreateMarker(markerPath, firstName, firstExisted, Path.GetFileName(firstBackup), secondName, secondExisted, Path.GetFileName(secondBackup));

            bool firstReplacementAttempted = false;
            bool secondReplacementAttempted = false;
            try
            {
                WriteDurable(firstStage, firstText);
                WriteDurable(secondStage, secondText);
                if (firstExisted) CopyDurable(firstPath, firstBackup, false);
                if (secondExisted) CopyDurable(secondPath, secondBackup, false);

                firstReplacementAttempted = true;
                Replace(firstStage, firstPath, firstExisted);
                secondReplacementAttempted = true;
                Replace(secondStage, secondPath, secondExisted);
            }
            catch (Exception originalFailure)
            {
                Exception rollbackFailure = null;
                if (secondReplacementAttempted)
                {
                    try { Restore(secondPath, secondBackup, secondExisted); }
                    catch (Exception failure) { rollbackFailure = failure; }
                }
                if (firstReplacementAttempted)
                {
                    try { Restore(firstPath, firstBackup, firstExisted); }
                    catch (Exception failure)
                    {
                        rollbackFailure = rollbackFailure == null ? failure : new AggregateException(rollbackFailure, failure);
                    }
                }

                if (rollbackFailure != null)
                    throw new AggregateException("Export pair replacement failed and rollback failed; keep C14Export.pending and its backups for manual recovery.", originalFailure, rollbackFailure);

                Exception cleanupFailure = Cleanup(firstStage, secondStage, firstBackup, secondBackup);
                if (cleanupFailure != null)
                    throw new AggregateException("Export pair failed and rollback completed, but temporary cleanup failed; keep C14Export.pending for inspection.", originalFailure, cleanupFailure);

                try { File.Delete(markerPath); }
                catch (Exception markerCleanupFailure)
                {
                    throw new AggregateException("Export pair failed and rollback completed, but C14Export.pending could not be removed.", originalFailure, markerCleanupFailure);
                }
                ExceptionDispatchInfo.Capture(originalFailure).Throw();
                throw;
            }

            Exception successCleanupFailure = Cleanup(firstStage, secondStage, firstBackup, secondBackup);
            if (successCleanupFailure != null)
                throw new IOException("Export pair was replaced, but backup cleanup failed; C14Export.pending remains to block mixed-snapshot builds.", successCleanupFailure);
            File.Delete(markerPath);
        }

        private static void ValidateName(string name)
        {
            if (string.IsNullOrEmpty(name) || name == "." || name == ".." ||
                name.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0 || Path.GetFileName(name) != name ||
                name.IndexOf('/') >= 0 || name.IndexOf('\\') >= 0 || name.IndexOf('\n') >= 0 || name.IndexOf('\r') >= 0)
                throw new ArgumentException("Export names must be simple file names.", nameof(name));
        }

        private static void RejectReadOnly(string path)
        {
            if (!File.Exists(path)) return;
            if ((File.GetAttributes(path) & FileAttributes.ReadOnly) != 0)
                throw new UnauthorizedAccessException("Export target is read-only: " + Path.GetFileName(path));
        }

        private static void CreateMarker(string path, string firstName, bool firstExisted, string firstBackup,
            string secondName, bool secondExisted, string secondBackup)
        {
            string manifest = "C14 export pair pending; inspect and recover before building.\n" +
                firstName + "\t" + firstExisted + "\t" + firstBackup + "\n" +
                secondName + "\t" + secondExisted + "\t" + secondBackup + "\n";
            FileStream stream = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.None);
            try
            {
                using (stream)
                {
                    byte[] bytes = Utf8.GetBytes(manifest);
                    stream.Write(bytes, 0, bytes.Length);
                    stream.Flush(true);
                }
            }
            catch (Exception markerFailure)
            {
                try { if (File.Exists(path)) File.Delete(path); }
                catch (Exception cleanupFailure)
                {
                    throw new AggregateException("Could not create a complete export marker and could not remove it; inspect C14Export.pending.", markerFailure, cleanupFailure);
                }
                throw;
            }
        }

        private static void WriteDurable(string path, string text, FileMode mode = FileMode.Create)
        {
            byte[] bytes = Utf8.GetBytes(text);
            using (var stream = new FileStream(path, mode, FileAccess.Write, FileShare.None))
            {
                stream.Write(bytes, 0, bytes.Length);
                stream.Flush(true);
            }
        }

        private static void CopyDurable(string source, string destination, bool overwrite)
        {
            using (var input = new FileStream(source, FileMode.Open, FileAccess.Read, FileShare.Read))
            using (var output = new FileStream(destination, overwrite ? FileMode.Create : FileMode.CreateNew, FileAccess.Write, FileShare.None))
            {
                input.CopyTo(output);
                output.Flush(true);
            }
        }

        private static void Replace(string stage, string target, bool targetExisted)
        {
            if (targetExisted) File.Replace(stage, target, null);
            else File.Move(stage, target);
        }

        private static void Restore(string target, string backup, bool existed)
        {
            if (existed) CopyDurable(backup, target, true);
            else if (File.Exists(target)) File.Delete(target);
        }

        private static Exception Cleanup(params string[] paths)
        {
            Exception failure = null;
            foreach (string path in paths)
            {
                try { if (File.Exists(path)) File.Delete(path); }
                catch (Exception error) { failure = failure == null ? error : new AggregateException(failure, error); }
            }
            return failure;
        }
    }
}
