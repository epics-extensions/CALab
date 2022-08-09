<?xml version='1.0'?>
<Project Type="Project" LVVersion="8508002">
	<Property Name="CCSymbols" Type="Str"></Property>
	<Property Name="NI.LV.All.SourceOnly" Type="Bool">false</Property>
	<Property Name="NI.Project.Description" Type="Str">main project of CA Lab V1.6.1.0
including all needed libraries
including basic examples</Property>
	<Item Name="My Computer" Type="My Computer">
		<Property Name="CCSymbols" Type="Str"></Property>
		<Property Name="server.app.propertiesEnabled" Type="Bool">true</Property>
		<Property Name="server.control.propertiesEnabled" Type="Bool">true</Property>
		<Property Name="server.tcp.acl" Type="Str">0800000008000000</Property>
		<Property Name="server.tcp.enabled" Type="Bool">false</Property>
		<Property Name="server.tcp.port" Type="Int">0</Property>
		<Property Name="server.tcp.serviceName" Type="Str"></Property>
		<Property Name="server.tcp.serviceName.default" Type="Str">My Computer/VI Server</Property>
		<Property Name="server.vi.access" Type="Str"></Property>
		<Property Name="server.vi.callsEnabled" Type="Bool">true</Property>
		<Property Name="server.vi.propertiesEnabled" Type="Bool">true</Property>
		<Property Name="server.viscripting.showScriptingOperationsInContextHelp" Type="Bool">false</Property>
		<Property Name="server.viscripting.showScriptingOperationsInEditor" Type="Bool">false</Property>
		<Property Name="specify.custom.address" Type="Bool">false</Property>
		<Item Name="caLab" Type="Folder">
			<Item Name="DemoIOC" Type="Folder">
				<Item Name="db" Type="Folder">
					<Item Name="demo.db" Type="Document" URL="/&lt;userlib&gt;/caLab/DemoIOC/db/demo.db"/>
					<Item Name="TestPV_ai100000.db" Type="Document" URL="/&lt;userlib&gt;/caLab/DemoIOC/db/TestPV_ai100000.db"/>
				</Item>
				<Item Name="DemoIOC.cmd" Type="Document" URL="/&lt;userlib&gt;/caLab/DemoIOC/DemoIOC.cmd"/>
				<Item Name="TestPV_ai100000.cmd" Type="Document" URL="/&lt;userlib&gt;/caLab/DemoIOC/TestPV_ai100000.cmd"/>
			</Item>
			<Item Name="Examples" Type="Folder">
				<Item Name="LV2019" Type="Folder">
					<Item Name="Parallel Event Demo Sub" Type="Folder">
						<Item Name="Close.vi" Type="VI" URL="Examples/LV2019/Parallel Event Demo Sub/Close.vi"/>
						<Item Name="Event Struct.vi" Type="VI" URL="Examples/LV2019/Parallel Event Demo Sub/Event Struct.vi"/>
						<Item Name="Indicator.vi" Type="VI" URL="Examples/LV2019/Parallel Event Demo Sub/Indicator.vi"/>
						<Item Name="Init.vi" Type="VI" URL="Examples/LV2019/Parallel Event Demo Sub/Init.vi"/>
						<Item Name="Task.vi" Type="VI" URL="Examples/LV2019/Parallel Event Demo Sub/Task.vi"/>
					</Item>
					<Item Name="Parallel Event Demo.vi" Type="VI" URL="Examples/LV2019/Parallel Event Demo.vi"/>
				</Item>
				<Item Name="caLab.db" Type="Document" URL="/&lt;userlib&gt;/caLab/Examples/caLab.db"/>
				<Item Name="Event Demo.vi" Type="VI" URL="Examples/Event Demo.vi"/>
				<Item Name="pvList.txt" Type="Document" URL="/&lt;userlib&gt;/caLab/Examples/pvList.txt"/>
				<Item Name="Read Demo 1.vi" Type="VI" URL="Examples/Read Demo 1.vi"/>
				<Item Name="Read Demo 2.vi" Type="VI" URL="Examples/Read Demo 2.vi"/>
				<Item Name="SoftIOC Demo Sub.vi" Type="VI" URL="Examples/SoftIOC Demo Sub.vi"/>
				<Item Name="SoftIOC Demo.vi" Type="VI" URL="Examples/SoftIOC Demo.vi"/>
				<Item Name="Write Demo - Looping.vi" Type="VI" URL="Examples/Write Demo - Looping.vi"/>
				<Item Name="Write Demo - Timed.vi" Type="VI" URL="Examples/Write Demo - Timed.vi"/>
				<Item Name="Write Demo.vi" Type="VI" URL="Examples/Write Demo.vi"/>
				<Item Name="Write Random TestPV_ai.vi" Type="VI" URL="Examples/Write Random TestPV_ai.vi"/>
			</Item>
			<Item Name="lib" Type="Folder"/>
			<Item Name="Private" Type="Folder"/>
			<Item Name="ReadMeFirst.txt" Type="Document" URL="/&lt;userlib&gt;/caLab/ReadMeFirst.txt"/>
			<Item Name="startDemo.bat" Type="Document" URL="/&lt;userlib&gt;/caLab/startDemo.bat"/>
			<Item Name="startTestPV_ai100000.bat" Type="Document" URL="/&lt;userlib&gt;/caLab/startTestPV_ai100000.bat"/>
		</Item>
		<Item Name="documentation" Type="Hyperlink">
			<Property Name="NI.Address" Type="Str">http://hz-b.de/calab</Property>
		</Item>
		<Item Name="Dependencies" Type="Dependencies">
			<Item Name="vi.lib" Type="Folder">
				<Item Name="Simple Error Handler.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Simple Error Handler.vi"/>
				<Item Name="DialogType.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/DialogType.ctl"/>
				<Item Name="General Error Handler.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/General Error Handler.vi"/>
				<Item Name="DialogTypeEnum.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/DialogTypeEnum.ctl"/>
				<Item Name="General Error Handler Core CORE.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/General Error Handler Core CORE.vi"/>
				<Item Name="whitespace.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/whitespace.ctl"/>
				<Item Name="Check Special Tags.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Check Special Tags.vi"/>
				<Item Name="TagReturnType.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/TagReturnType.ctl"/>
				<Item Name="Set String Value.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Set String Value.vi"/>
				<Item Name="GetRTHostConnectedProp.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/GetRTHostConnectedProp.vi"/>
				<Item Name="Error Code Database.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Error Code Database.vi"/>
				<Item Name="Trim Whitespace.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Trim Whitespace.vi"/>
				<Item Name="Format Message String.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Format Message String.vi"/>
				<Item Name="Find Tag.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Find Tag.vi"/>
				<Item Name="Search and Replace Pattern.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Search and Replace Pattern.vi"/>
				<Item Name="Set Bold Text.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Set Bold Text.vi"/>
				<Item Name="Details Display Dialog.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Details Display Dialog.vi"/>
				<Item Name="ErrWarn.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/ErrWarn.ctl"/>
				<Item Name="eventvkey.ctl" Type="VI" URL="/&lt;vilib&gt;/event_ctls.llb/eventvkey.ctl"/>
				<Item Name="Clear Errors.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Clear Errors.vi"/>
				<Item Name="Not Found Dialog.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Not Found Dialog.vi"/>
				<Item Name="Three Button Dialog.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Three Button Dialog.vi"/>
				<Item Name="Three Button Dialog CORE.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Three Button Dialog CORE.vi"/>
				<Item Name="LVRectTypeDef.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/miscctls.llb/LVRectTypeDef.ctl"/>
				<Item Name="Longest Line Length in Pixels.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Longest Line Length in Pixels.vi"/>
				<Item Name="Convert property node font to graphics font.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Convert property node font to graphics font.vi"/>
				<Item Name="Get Text Rect.vi" Type="VI" URL="/&lt;vilib&gt;/picture/picture.llb/Get Text Rect.vi"/>
				<Item Name="Get String Text Bounds.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Get String Text Bounds.vi"/>
				<Item Name="LVBoundsTypeDef.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/miscctls.llb/LVBoundsTypeDef.ctl"/>
				<Item Name="BuildHelpPath.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/BuildHelpPath.vi"/>
				<Item Name="GetHelpDir.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/GetHelpDir.vi"/>
				<Item Name="Check if File or Folder Exists.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/libraryn.llb/Check if File or Folder Exists.vi"/>
				<Item Name="NI_FileType.lvlib" Type="Library" URL="/&lt;vilib&gt;/Utility/lvfile.llb/NI_FileType.lvlib"/>
				<Item Name="Error Cluster From Error Code.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Error Cluster From Error Code.vi"/>
				<Item Name="NI_PackedLibraryUtility.lvlib" Type="Library" URL="/&lt;vilib&gt;/Utility/LVLibp/NI_PackedLibraryUtility.lvlib"/>
				<Item Name="subFile Dialog.vi" Type="VI" URL="/&lt;vilib&gt;/express/express input/FileDialogBlock.llb/subFile Dialog.vi"/>
				<Item Name="ex_CorrectErrorChain.vi" Type="VI" URL="/&lt;vilib&gt;/express/express shared/ex_CorrectErrorChain.vi"/>
				<Item Name="System Exec.vi" Type="VI" URL="/&lt;vilib&gt;/Platform/system.llb/System Exec.vi"/>
				<Item Name="Set Busy.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/cursorutil.llb/Set Busy.vi"/>
				<Item Name="Set Cursor.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/cursorutil.llb/Set Cursor.vi"/>
				<Item Name="Set Cursor (Cursor ID).vi" Type="VI" URL="/&lt;vilib&gt;/Utility/cursorutil.llb/Set Cursor (Cursor ID).vi"/>
				<Item Name="Set Cursor (Icon Pict).vi" Type="VI" URL="/&lt;vilib&gt;/Utility/cursorutil.llb/Set Cursor (Icon Pict).vi"/>
				<Item Name="Unset Busy.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/cursorutil.llb/Unset Busy.vi"/>
				<Item Name="Random Number (Range).vi" Type="VI" URL="/&lt;vilib&gt;/numeric/Random Number (Range).vi"/>
				<Item Name="Random Number (Range) DBL.vi" Type="VI" URL="/&lt;vilib&gt;/numeric/Random Number (Range) DBL.vi"/>
				<Item Name="Random Number (Range) I64.vi" Type="VI" URL="/&lt;vilib&gt;/numeric/Random Number (Range) I64.vi"/>
				<Item Name="sub_Random U32.vi" Type="VI" URL="/&lt;vilib&gt;/numeric/sub_Random U32.vi"/>
				<Item Name="Random Number (Range) U64.vi" Type="VI" URL="/&lt;vilib&gt;/numeric/Random Number (Range) U64.vi"/>
			</Item>
			<Item Name="user.lib" Type="Folder">
				<Item Name="CaLab.lvlib" Type="Library" URL="CaLab.lvlib"/>
			</Item>
		</Item>
		<Item Name="Build Specifications" Type="Build"/>
	</Item>
</Project>
