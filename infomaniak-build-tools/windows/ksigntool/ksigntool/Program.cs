using System;
using System.Diagnostics;
using System.Linq;
using System.Windows.Automation;

namespace AutoSafeNetLogon
{
    class Program
    {
        static void Main(string[] args)
        {
            // Find and extract the password argument
            string passwordArg = args.FirstOrDefault(a => a.StartsWith("/password:", StringComparison.OrdinalIgnoreCase));
            string password = "";
            if (passwordArg != null)
            {
                password = passwordArg.Substring("/password:".Length);
            }

            if (!string.IsNullOrWhiteSpace(password))
            {
                // Add an automation event handler to handle SafeNet token password requests
                SatisfyEverySafeNetTokenPasswordRequest(password);
            }
            else
            {
                Console.WriteLine("Missing /password:<pwd> argument, if the the certificate is on a SafeNet token, you will be prompted for the password.");
            }

            // Remove password from args before calling signtool
            var remainingArgs = args.Where(a => !a.StartsWith("/password:", StringComparison.OrdinalIgnoreCase)).ToArray();

            // Run signtool with the remaining args<
            var startInfo = new ProcessStartInfo
            {
                FileName = "signtool.exe",
                Arguments = string.Join(" ", remainingArgs),
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
            };

            try
            {
                Console.WriteLine("Signtool starting with arguments: " + startInfo.Arguments);
                var process = Process.Start(startInfo);
                process.OutputDataReceived += (sender, e) => Console.WriteLine(e.Data);
                process.ErrorDataReceived += (sender, e) => Console.Error.WriteLine(e.Data);
                process.BeginOutputReadLine();
                process.BeginErrorReadLine();
                process.WaitForExit();
                if (process.ExitCode != 0)
                {
                    Console.Error.WriteLine($"Signtool exited with code {process.ExitCode}");
                    Environment.Exit(process.ExitCode);
                }
                else
                {
                    Console.WriteLine("Signtool completed successfully.");
                }
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Failed to start signtool: {ex.Message}");
            }
            Console.WriteLine("ksigntool finished");
            // Clear automation handlers
            Automation.RemoveAllEventHandlers();
        }

        static void SatisfyEverySafeNetTokenPasswordRequest(string password)
        {
            int count = 0;
            Automation.AddAutomationEventHandler(WindowPattern.WindowOpenedEvent, AutomationElement.RootElement, TreeScope.Children, (sender, e) =>
            {
                var element = sender as AutomationElement;
                if (element.Current.Name == "Token Logon" || element.Current.Name == "Connexion au token")
                {
                    WindowPattern pattern = (WindowPattern)element.GetCurrentPattern(WindowPattern.Pattern);
                    pattern.WaitForInputIdle(10000);
                    var edit = element.FindFirst(TreeScope.Descendants, new AndCondition(
                        new PropertyCondition(AutomationElement.ControlTypeProperty, ControlType.Edit),
                        new PropertyCondition(AutomationElement.NameProperty, "Token Password:")));
                    if (edit == null)
                    {
                        edit = element.FindFirst(TreeScope.Descendants, new AndCondition(
                            new PropertyCondition(AutomationElement.ControlTypeProperty, ControlType.Edit),
                            new PropertyCondition(AutomationElement.NameProperty, "Mot de passe du token:")));
                    }

                    var ok = element.FindFirst(TreeScope.Descendants, new AndCondition(
                        new PropertyCondition(AutomationElement.ControlTypeProperty, ControlType.Button),
                        new PropertyCondition(AutomationElement.NameProperty, "OK")));

                    if (edit != null && ok != null)
                    {
                        count++;
                        ValuePattern vp = (ValuePattern)edit.GetCurrentPattern(ValuePattern.Pattern);
                        vp.SetValue(password);
                        Console.WriteLine("SafeNet window (count: " + count + " window(s)) detected. Setting password...");

                        InvokePattern ip = (InvokePattern)ok.GetCurrentPattern(InvokePattern.Pattern);
                        ip.Invoke();
                    }
                    else
                    {
                        Console.WriteLine("SafeNet window detected but not with edit and button...");
                    }
                }
            });
        }
    }
}