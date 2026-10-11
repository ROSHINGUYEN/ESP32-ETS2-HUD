Set WshShell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
toolsDir = fso.GetParentFolderName(WScript.ScriptFullName)
repoDir = fso.GetParentFolderName(toolsDir)

WshShell.CurrentDirectory = repoDir
pythonExe = "C:\Users\Tinhd\AppData\Local\Programs\Python\Python311\python.exe"
pythonScript = repoDir & "\tools\miu_desktop_ets2.py"

cmd = """" & pythonExe & """ """ & pythonScript & """"
WshShell.Run cmd, 0, False
