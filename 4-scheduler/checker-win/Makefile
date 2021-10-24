build: libscheduler.dll

libscheduler.dll: tema_windows.obj priority_queue.obj
	link /nologo /dll /out:libscheduler.dll tema_windows.obj priority_queue.obj

tema_windows.obj: tema_windows.c
	cl /nologo /W3 /D_CRT_SECURE_NO_DEPRECATE /DDLL_IMPORTS /Fotema_windows.obj /c tema_windows.c

priority_queue.obj: priority_queue.c
	cl /nologo /W3 /D_CRT_SECURE_NO_DEPRECATE /DDLL_IMPORTS /Fopriority_queue.obj /c priority_queue.c

clean:
	del /Q /F *.obj *.lib *.dll *.exe  *.exp