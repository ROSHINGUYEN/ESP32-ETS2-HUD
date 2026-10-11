Set WshShell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")

' Tim thu muc goc repo chua tools\miu_desktop_ets2.py
repoDir = "E:\MIU_EILIK_ESP32-S3"

scriptPath = WScript.ScriptFullName
parentDir = fso.GetParentFolderName(scriptPath)
grandDir = fso.GetParentFolderName(parentDir)

if fso.FileExists(parentDir & "\miu_desktop_ets2.py") then
    repoDir = grandDir
elseif fso.FileExists(parentDir & "\tools\miu_desktop_ets2.py") then
    repoDir = parentDir
end if

if not fso.FolderExists(repoDir) then
    repoDir = "E:\MIU_EILIK_ESP32-S3"
end if

WshShell.CurrentDirectory = repoDir

pythonExe = "C:\Users\Tinhd\AppData\Local\Programs\Python\Python311\pythonw.exe"
if not fso.FileExists(pythonExe) then
    pythonExe = "C:\Users\Tinhd\AppData\Local\Programs\Python\Python311\python.exe"
end if

pythonScript = repoDir & "\tools\miu_desktop_ets2.py"
cmd = """" & pythonExe & """ """ & pythonScript & """"
WshShell.Run cmd, 0, False
