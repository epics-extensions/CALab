<?xml version='1.0' encoding='UTF-8'?>
<Project Type="Project" LVVersion="19008000">
	<Property Name="NI.LV.All.SourceOnly" Type="Bool">true</Property>
	<Property Name="NI.Project.Description" Type="Str">Examples of CA Lab
https://github.com/epics-extensions/CALab/</Property>
	<Item Name="My Computer" Type="My Computer">
		<Property Name="server.app.propertiesEnabled" Type="Bool">true</Property>
		<Property Name="server.control.propertiesEnabled" Type="Bool">true</Property>
		<Property Name="server.tcp.enabled" Type="Bool">false</Property>
		<Property Name="server.tcp.port" Type="Int">0</Property>
		<Property Name="server.tcp.serviceName" Type="Str">My Computer/VI Server</Property>
		<Property Name="server.tcp.serviceName.default" Type="Str">My Computer/VI Server</Property>
		<Property Name="server.vi.callsEnabled" Type="Bool">true</Property>
		<Property Name="server.vi.propertiesEnabled" Type="Bool">true</Property>
		<Property Name="specify.custom.address" Type="Bool">false</Property>
		<Item Name="Examples" Type="Folder">
			<Item Name="Parallel Event Demo Sub" Type="Folder">
				<Item Name="Parallel Event Close.vi" Type="VI" URL="/&lt;userlib&gt;/calab/Examples/Parallel Event Demo Sub/Parallel Event Close.vi"/>
				<Item Name="Parallel Event Indicator.vi" Type="VI" URL="/&lt;userlib&gt;/calab/Examples/Parallel Event Demo Sub/Parallel Event Indicator.vi"/>
				<Item Name="Parallel Event Init.vi" Type="VI" URL="/&lt;userlib&gt;/calab/Examples/Parallel Event Demo Sub/Parallel Event Init.vi"/>
				<Item Name="Parallel Event Struct.vi" Type="VI" URL="/&lt;userlib&gt;/calab/Examples/Parallel Event Demo Sub/Parallel Event Struct.vi"/>
				<Item Name="Parallel Event Task.vi" Type="VI" URL="/&lt;userlib&gt;/calab/Examples/Parallel Event Demo Sub/Parallel Event Task.vi"/>
			</Item>
			<Item Name="caLab.db" Type="Document" URL="/&lt;userlib&gt;/caLab/Examples/caLab.db"/>
			<Item Name="Event Demo.vi" Type="VI" URL="/&lt;userlib&gt;/calab/Examples/Event Demo.vi"/>
			<Item Name="Parallel Event Demo.vi" Type="VI" URL="/&lt;userlib&gt;/calab/Examples/Parallel Event Demo.vi"/>
			<Item Name="pvList.txt" Type="Document" URL="/&lt;userlib&gt;/caLab/Examples/pvList.txt"/>
			<Item Name="Read Demo 1.vi" Type="VI" URL="/&lt;userlib&gt;/calab/Examples/Read Demo 1.vi"/>
			<Item Name="Read Demo 2.vi" Type="VI" URL="/&lt;userlib&gt;/calab/Examples/Read Demo 2.vi"/>
			<Item Name="SoftIOC Demo Sub.vi" Type="VI" URL="/&lt;userlib&gt;/calab/Examples/SoftIOC Demo Sub.vi"/>
			<Item Name="SoftIOC Demo.vi" Type="VI" URL="/&lt;userlib&gt;/calab/Examples/SoftIOC Demo.vi"/>
			<Item Name="Write Demo - Looping.vi" Type="VI" URL="/&lt;userlib&gt;/calab/Examples/Write Demo - Looping.vi"/>
			<Item Name="Write Demo - Timed.vi" Type="VI" URL="/&lt;userlib&gt;/calab/Examples/Write Demo - Timed.vi"/>
			<Item Name="Write Demo.vi" Type="VI" URL="/&lt;userlib&gt;/calab/Examples/Write Demo.vi"/>
			<Item Name="Write Random TestPV_ai.vi" Type="VI" URL="/&lt;userlib&gt;/calab/Examples/Write Random TestPV_ai.vi"/>
		</Item>
		<Item Name="Lib" Type="Folder">
			<Item Name="ca.dll" Type="Document" URL="/&lt;userlib&gt;/caLab/Lib/ca.dll"/>
			<Item Name="caget.exe" Type="Document" URL="/&lt;userlib&gt;/caLab/Lib/caget.exe"/>
			<Item Name="cainfo.exe" Type="Document" URL="/&lt;userlib&gt;/caLab/Lib/cainfo.exe"/>
			<Item Name="calab.dll" Type="Document" URL="/&lt;userlib&gt;/caLab/Lib/calab.dll"/>
			<Item Name="camonitor.exe" Type="Document" URL="/&lt;userlib&gt;/caLab/Lib/camonitor.exe"/>
			<Item Name="caput.exe" Type="Document" URL="/&lt;userlib&gt;/caLab/Lib/caput.exe"/>
			<Item Name="caRepeater.exe" Type="Document" URL="/&lt;userlib&gt;/caLab/Lib/caRepeater.exe"/>
			<Item Name="Com.dll" Type="Document" URL="/&lt;userlib&gt;/caLab/Lib/Com.dll"/>
			<Item Name="copy_epics_7_0_7_debug_libs.bat" Type="Document" URL="/&lt;userlib&gt;/caLab/Lib/copy_epics_7_0_7_debug_libs.bat"/>
			<Item Name="copy_epics_7_0_7_release_libs.bat" Type="Document" URL="/&lt;userlib&gt;/caLab/Lib/copy_epics_7_0_7_release_libs.bat"/>
			<Item Name="dbCore.dll" Type="Document" URL="/&lt;userlib&gt;/caLab/Lib/dbCore.dll"/>
			<Item Name="dbRecStd.dll" Type="Document" URL="/&lt;userlib&gt;/caLab/Lib/dbRecStd.dll"/>
			<Item Name="softIoc.dbd" Type="Document" URL="/&lt;userlib&gt;/caLab/Lib/softIoc.dbd"/>
			<Item Name="softIoc.exe" Type="Document" URL="/&lt;userlib&gt;/caLab/Lib/softIoc.exe"/>
		</Item>
		<Item Name="CaLab.lvlib" Type="Library" URL="/&lt;userlib&gt;/calab/CaLab.lvlib"/>
		<Item Name="Dependencies" Type="Dependencies">
			<Item Name="vi.lib" Type="Folder">
				<Item Name="Application Directory.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/file.llb/Application Directory.vi"/>
				<Item Name="BuildHelpPath.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/BuildHelpPath.vi"/>
				<Item Name="Check if File or Folder Exists.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/libraryn.llb/Check if File or Folder Exists.vi"/>
				<Item Name="Check Special Tags.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Check Special Tags.vi"/>
				<Item Name="Clear Errors.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Clear Errors.vi"/>
				<Item Name="Convert property node font to graphics font.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Convert property node font to graphics font.vi"/>
				<Item Name="Details Display Dialog.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Details Display Dialog.vi"/>
				<Item Name="DialogType.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/DialogType.ctl"/>
				<Item Name="DialogTypeEnum.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/DialogTypeEnum.ctl"/>
				<Item Name="Error Cluster From Error Code.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Error Cluster From Error Code.vi"/>
				<Item Name="Error Code Database.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Error Code Database.vi"/>
				<Item Name="ErrWarn.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/ErrWarn.ctl"/>
				<Item Name="eventvkey.ctl" Type="VI" URL="/&lt;vilib&gt;/event_ctls.llb/eventvkey.ctl"/>
				<Item Name="ex_CorrectErrorChain.vi" Type="VI" URL="/&lt;vilib&gt;/express/express shared/ex_CorrectErrorChain.vi"/>
				<Item Name="Find Tag.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Find Tag.vi"/>
				<Item Name="Format Message String.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Format Message String.vi"/>
				<Item Name="General Error Handler Core CORE.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/General Error Handler Core CORE.vi"/>
				<Item Name="General Error Handler.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/General Error Handler.vi"/>
				<Item Name="Get String Text Bounds.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Get String Text Bounds.vi"/>
				<Item Name="Get Text Rect.vi" Type="VI" URL="/&lt;vilib&gt;/picture/picture.llb/Get Text Rect.vi"/>
				<Item Name="GetHelpDir.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/GetHelpDir.vi"/>
				<Item Name="GetRTHostConnectedProp.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/GetRTHostConnectedProp.vi"/>
				<Item Name="Longest Line Length in Pixels.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Longest Line Length in Pixels.vi"/>
				<Item Name="LVBoundsTypeDef.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/miscctls.llb/LVBoundsTypeDef.ctl"/>
				<Item Name="LVRectTypeDef.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/miscctls.llb/LVRectTypeDef.ctl"/>
				<Item Name="NI_FileType.lvlib" Type="Library" URL="/&lt;vilib&gt;/Utility/lvfile.llb/NI_FileType.lvlib"/>
				<Item Name="NI_PackedLibraryUtility.lvlib" Type="Library" URL="/&lt;vilib&gt;/Utility/LVLibp/NI_PackedLibraryUtility.lvlib"/>
				<Item Name="Not Found Dialog.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Not Found Dialog.vi"/>
				<Item Name="Search and Replace Pattern.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Search and Replace Pattern.vi"/>
				<Item Name="Set Bold Text.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Set Bold Text.vi"/>
				<Item Name="Set Busy.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/cursorutil.llb/Set Busy.vi"/>
				<Item Name="Set Cursor (Cursor ID).vi" Type="VI" URL="/&lt;vilib&gt;/Utility/cursorutil.llb/Set Cursor (Cursor ID).vi"/>
				<Item Name="Set Cursor (Icon Pict).vi" Type="VI" URL="/&lt;vilib&gt;/Utility/cursorutil.llb/Set Cursor (Icon Pict).vi"/>
				<Item Name="Set Cursor.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/cursorutil.llb/Set Cursor.vi"/>
				<Item Name="Set String Value.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Set String Value.vi"/>
				<Item Name="Simple Error Handler.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Simple Error Handler.vi"/>
				<Item Name="subFile Dialog.vi" Type="VI" URL="/&lt;vilib&gt;/express/express input/FileDialogBlock.llb/subFile Dialog.vi"/>
				<Item Name="System Exec.vi" Type="VI" URL="/&lt;vilib&gt;/Platform/system.llb/System Exec.vi"/>
				<Item Name="TagReturnType.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/TagReturnType.ctl"/>
				<Item Name="Three Button Dialog CORE.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Three Button Dialog CORE.vi"/>
				<Item Name="Three Button Dialog.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Three Button Dialog.vi"/>
				<Item Name="Trim Whitespace.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Trim Whitespace.vi"/>
				<Item Name="Unset Busy.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/cursorutil.llb/Unset Busy.vi"/>
				<Item Name="whitespace.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/whitespace.ctl"/>
			</Item>
		</Item>
		<Item Name="Build Specifications" Type="Build"/>
	</Item>
</Project>
