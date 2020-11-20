#include "stdafx.h"

struct Vertex {
	Vertex(float x, float y, float z, float u, float v) : pos(x, y, z), texCoord(u, v) {}
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT2 texCoord;
};

int LoadImageDataFromFile(BYTE** imageData, D3D12_RESOURCE_DESC& resourceDescription, LPCWSTR filename, int& bytesPerRow) {
	HRESULT hr;

	// use wic factory to create bitmap decoders
	static IWICImagingFactory* wicFactory;

	IWICBitmapDecoder* wicDecoder = NULL;
	IWICBitmapFrameDecode* wicFrame = NULL;
	IWICFormatConverter* wicConverter = NULL;

	bool imageConverted = false;

	if (wicFactory == NULL)
	{
		// initialize Com library which contains functions for creation of com applications
		// parameter must be null
		CoInitialize(NULL);

		hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));
		if (FAILED(hr))
			return 0;
	}

	hr = wicFactory->CreateDecoderFromFilename(filename, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &wicDecoder); 
	if (FAILED(hr))
		return 0;

	// get first frame since some files may be gifs
	hr = wicDecoder->GetFrame(0, &wicFrame);
	if (FAILED(hr))
		return 0;

	WICPixelFormatGUID pixelFormat;
	hr = wicFrame->GetPixelFormat(&pixelFormat);
	if (FAILED(hr))
		return 0;

	UINT textureWidth, textureHeight;
	hr = wicFrame->GetSize(&textureWidth, &textureHeight);
	if (FAILED(hr))
		return 0;

	// checks for DXGI format compatibility
	DXGI_FORMAT dxgiFormat = GetDXGIFormatFromWICFormat(pixelFormat);

	// if not compatible
	if (dxgiFormat == DXGI_FORMAT_UNKNOWN)
	{
		// get dxgi compatible wic format
		WICPixelFormatGUID convertToPixelFormat = GetConvertToWICFormat(pixelFormat);

		// if can't then return
		if (convertToPixelFormat == GUID_WICPixelFormatDontCare)
			return 0;

		dxgiFormat = GetDXGIFormatFromWICFormat(convertToPixelFormat);

		hr = wicFactory->CreateFormatConverter(&wicConverter);
		if (FAILED(hr))
			return 0;

		BOOL canConvert = FALSE;
		hr = wicConverter->CanConvert(pixelFormat, convertToPixelFormat, &canConvert);
		if (FAILED(hr))
			return 0;

		hr = wicConverter->Initialize(wicFrame, convertToPixelFormat, WICBitmapDitherTypeErrorDiffusion, 0, 0, WICBitmapPaletteTypeCustom);
		if (FAILED(hr))
			return 0;

		imageConverted = true;
	}

	int bitsPerPixel = GetDXGIFormatBitsPerPixel(dxgiFormat);
	bytesPerRow = (textureWidth * bitsPerPixel) / 8;
	// image size in bytes
	int imageSize = bytesPerRow * textureHeight;

	*imageData = (BYTE*)malloc(imageSize);

	// copy image data into allocated memory
	if (imageConverted) {
		// if converted, then use the wic converter
		hr = wicConverter->CopyPixels(0, bytesPerRow, imageSize, *imageData);
		if (FAILED(hr))
			return 0;
	}
	else {
		// if not converted then copy straight from frame
		hr = wicFrame->CopyPixels(0, bytesPerRow, imageSize, *imageData);
		if (FAILED(hr))
			return 0;
	}

	resourceDescription = {};
	// type of resource
	resourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	// zero is automatic alignment, but should be explicitly set for better mapping to heaps
	resourceDescription.Alignment = 0;
	resourceDescription.Width = textureWidth;
	resourceDescription.Height = textureHeight;
	// one image, also not a 3d image
	resourceDescription.DepthOrArraySize = 1;
	// no mipmaps
	resourceDescription.MipLevels = 1;
	// previously converted to dxgi format
	resourceDescription.Format = dxgiFormat;
	resourceDescription.SampleDesc.Count = 1;
	resourceDescription.SampleDesc.Quality = 0;
	// driver chooses the most efficient pixel layout (linear vs swizzle)
	resourceDescription.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

	return imageSize;
}

bool InitializeWindow(HINSTANCE hInstance, int ShowWnd, bool fullscreen) {
	if (fullscreen) {
		// monitor handler
		// gets handle to display monitor that has largest area of intersection with "window"
		HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
		// sets cbSize to struct size in order for GetMonitorInfo to work
		MONITORINFO mi = { sizeof(mi) };
		GetMonitorInfo(hmon, &mi);

		Width = mi.rcMonitor.right - mi.rcMonitor.left;
		Height = mi.rcMonitor.bottom - mi.rcMonitor.top;
	}

	// different from WNDCLASS since it has size of structure as well as member for icon
	WNDCLASSEX wc;

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = NULL;
	wc.cbWndExtra = NULL;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = WindowName;
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	if (!RegisterClassEx(&wc)) {
		MessageBox(NULL, L"Error Registering class", L"Error", MB_OK | MB_ICONERROR);
		return false;
	}

	hwnd = CreateWindowEx(NULL,
		WindowName,
		WindowTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		Width, Height,
		NULL,
		NULL,
		hInstance,
		NULL);

	if (!hwnd)
	{
		MessageBox(NULL, L"Error creating window.", L"Error", MB_OK | MB_ICONERROR);
		return false;
	}

	if (fullscreen)
	{
		// Get Window Long
		SetWindowLong(hwnd, GWL_STYLE, 0);
	}

	ShowWindow(hwnd, ShowWnd);
	UpdateWindow(hwnd);

	return true;
}

void mainloop() {
	MSG msg;
	// fill size of dedicated memory with zeroes
	ZeroMemory(&msg, sizeof(MSG));

	while (Running)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT)
				break;

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			Update();
			Render();
		}
	}
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE) {
			if (MessageBox(0, L"Exit?", L"Exit?", MB_YESNO | MB_ICONQUESTION) == IDYES) {
				Running = false;
				DestroyWindow(hwnd);
			}
		}
		return 0;
	case WM_DESTROY:
		Running = false;
		PostQuitMessage(0);
		return 0;
	default:
		// default window procedure
		// not exactly necessary unless window procedure is modified in any way
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return 0;
}

bool InitD3D() {
	// used for exception handling
	HRESULT hr;

	// object enables creation of dxgi objects
	IDXGIFactory4* dxgiFactory;
	// 1 stands for version 1.1
	// IID_PPV_ARGS macro prevents type mismatch
	hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
	if (FAILED(hr))
		return false;

	// adapters are graphics cards (including integrated graphics)
	IDXGIAdapter1* adapter;

	int adapterIndex = 0;
	bool adapterFound = false;

	while (dxgiFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND) {
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		// bitwise and for software device check
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
			adapterIndex++;
			continue;
		}
		
		// check if device works with feature level 11 for direct3d 12
		// null 4th parameter because only checking if the device exists
		hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr);
		if (SUCCEEDED(hr))
		{
			adapterFound = true;
			break;
		}

		adapterIndex++;
	}

	if (!adapterFound)
		return false;


	hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
	if (FAILED(hr))
		return false;

	D3D12_COMMAND_QUEUE_DESC cqDesc = {};
	cqDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	hr = device->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&commandQueue));
	if (FAILED(hr))
		return false;

	DXGI_MODE_DESC backBufferDesc = {};
	backBufferDesc.Width = Width;
	backBufferDesc.Height = Height;
	// rgba 32 bits
	backBufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	DXGI_SAMPLE_DESC sampleDesc = {};
	// no multisampling
	sampleDesc.Count = 1;

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferCount = frameBufferCount;
	swapChainDesc.BufferDesc = backBufferDesc;
	// render target or shader input, but usually render target
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	// discard buffer
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.OutputWindow = hwnd;
	swapChainDesc.SampleDesc = sampleDesc;
	swapChainDesc.Windowed = !FullScreen;

	IDXGISwapChain* tempSwapChain;

	dxgiFactory->CreateSwapChain(commandQueue, &swapChainDesc, &tempSwapChain);

	// static cast is used to get current back buffer, since CreateSwapChain requires IDXGISwapChain
	swapChain = static_cast<IDXGISwapChain3*>(tempSwapChain);

	// setting initial frame index
	frameIndex = swapChain->GetCurrentBackBufferIndex();

	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = frameBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap));
	if (FAILED(hr))
		return false;

	// descriptor sizes vary from device to device
	rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// get "pointer" to first rtv in list
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	for (int i = 0; i < frameBufferCount; ++i) {
		hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]));
		if (FAILED(hr))
			return false;

		device->CreateRenderTargetView(renderTargets[i], nullptr, rtvHandle);
		// moves handle along ("increments")
		rtvHandle.Offset(1, rtvDescriptorSize);
	}

	for (int i = 0; i < frameBufferCount; ++i) {
		hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator[i]));
		if (FAILED(hr))
			return false;
	}

	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator[0], NULL, IID_PPV_ARGS(&commandList));
	if (FAILED(hr))
		return false;

	// when command lists are created, they are created in the recording state, so they must be closed
	// close later after recording
	//commandList->Close();

	for (int i = 0; i < frameBufferCount; i++) {
		// fence initial value is 0
		hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence[i]));
		if (FAILED(hr))
			return false;

		fenceValue[i] = 0;
	}

	fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fenceEvent == nullptr)
		return false;

	// create root parameters before adding them into the root signature
	
	D3D12_DESCRIPTOR_RANGE descriptorTableRanges[1];
	descriptorTableRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorTableRanges[0].NumDescriptors = 1;
	descriptorTableRanges[0].BaseShaderRegister = 0;
	descriptorTableRanges[0].RegisterSpace = 0;
	descriptorTableRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_DESCRIPTOR_TABLE descriptorTable;
	descriptorTable.NumDescriptorRanges = _countof(descriptorTableRanges);
	descriptorTable.pDescriptorRanges = &descriptorTableRanges[0];


	// new root parameter will support versioning
	// versioning is necessary for multiple objects with different data for the constant buffer
	// because different data per object means updating the constant buffer multiple times per command list

	D3D12_ROOT_DESCRIPTOR rootCBVDescriptor;
	rootCBVDescriptor.RegisterSpace = 0;
	rootCBVDescriptor.ShaderRegister = 0;

	D3D12_ROOT_PARAMETER rootParameters[2];
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].Descriptor = rootCBVDescriptor;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].DescriptorTable = descriptorTable;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	
	// static samplers are more performant, but cannot be changed
	// only here to make code easy
	D3D12_STATIC_SAMPLER_DESC sampler = {};
	// point sampler
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	// set to border color below
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler.MipLODBias = 0;
	sampler.MaxAnisotropy = 0;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler.MinLOD = 0.0f;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init(_countof(rootParameters), 
		rootParameters, 1, 
		&sampler, 
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);

	ID3DBlob* signature;
	// serialize root signature into bytecode
	hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr);
	if (FAILED(hr))
		return false;

	hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	if (FAILED(hr))
		return false;

	// blobs store data of arbitrary length
	// this blob stores the vertex shader bytecode
	ID3DBlob* vertexShader;
	// blob to see error if there is one
	ID3DBlob* errorBuffer;
	hr = D3DCompileFromFile(L"VertexShader.hlsl", nullptr, nullptr, "main", "vs_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &vertexShader, &errorBuffer);
	if (FAILED(hr)) {
		OutputDebugStringA((char*)errorBuffer->GetBufferPointer());
		return false;
	}

	D3D12_SHADER_BYTECODE vertexShaderBytecode = {};
	vertexShaderBytecode.BytecodeLength = vertexShader->GetBufferSize();
	vertexShaderBytecode.pShaderBytecode = vertexShader->GetBufferPointer();

	ID3DBlob* pixelShader;
	hr = D3DCompileFromFile(L"PixelShader.hlsl", nullptr, nullptr, "main", "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &pixelShader, &errorBuffer);
	if (FAILED(hr)) {
		OutputDebugStringA((char*)errorBuffer->GetBufferPointer());
		return false;
	}

	D3D12_SHADER_BYTECODE pixelShaderBytecode = {};
	pixelShaderBytecode.BytecodeLength = pixelShader->GetBufferSize();
	pixelShaderBytecode.pShaderBytecode = pixelShader->GetBufferPointer();

	// this includes information such as position, uv coords
	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};

	inputLayoutDesc.NumElements = sizeof(inputLayout) / sizeof(D3D12_INPUT_ELEMENT_DESC);
	inputLayoutDesc.pInputElementDescs = inputLayout;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = inputLayoutDesc;
	psoDesc.pRootSignature = rootSignature;
	psoDesc.VS = vertexShaderBytecode;
	psoDesc.PS = pixelShaderBytecode;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc = sampleDesc;
	// point sampling
	psoDesc.SampleMask = 0xffffffff;
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.NumRenderTargets = 1;
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

	hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineStateObject));
	if (FAILED(hr))
		return false;

	Vertex vList[] = {
		// front face
		{ -0.5f,  0.5f, -0.5f, 0.0f, 0.0f },
		{  0.5f, -0.5f, -0.5f, 1.0f, 1.0f },
		{ -0.5f, -0.5f, -0.5f, 0.0f, 1.0f },
		{  0.5f,  0.5f, -0.5f, 1.0f, 0.0f },

		// right side face
		{  0.5f, -0.5f, -0.5f, 0.0f, 1.0f },
		{  0.5f,  0.5f,  0.5f, 1.0f, 0.0f },
		{  0.5f, -0.5f,  0.5f, 1.0f, 1.0f },
		{  0.5f,  0.5f, -0.5f, 0.0f, 0.0f },

		// left side face
		{ -0.5f,  0.5f,  0.5f, 0.0f, 0.0f },
		{ -0.5f, -0.5f, -0.5f, 1.0f, 1.0f },
		{ -0.5f, -0.5f,  0.5f, 0.0f, 1.0f },
		{ -0.5f,  0.5f, -0.5f, 1.0f, 0.0f },

		// back face
		{  0.5f,  0.5f,  0.5f, 0.0f, 0.0f },
		{ -0.5f, -0.5f,  0.5f, 1.0f, 1.0f },
		{  0.5f, -0.5f,  0.5f, 0.0f, 1.0f },
		{ -0.5f,  0.5f,  0.5f, 1.0f, 0.0f },

		// top face
		{ -0.5f,  0.5f, -0.5f, 0.0f, 1.0f },
		{  0.5f,  0.5f,  0.5f, 1.0f, 0.0f },
		{  0.5f,  0.5f, -0.5f, 1.0f, 1.0f },
		{ -0.5f,  0.5f,  0.5f, 0.0f, 0.0f },

		// bottom face
		{  0.5f, -0.5f,  0.5f, 0.0f, 0.0f },
		{ -0.5f, -0.5f, -0.5f, 1.0f, 1.0f },
		{  0.5f, -0.5f, -0.5f, 0.0f, 1.0f },
		{ -0.5f, -0.5f,  0.5f, 1.0f, 0.0f },
	};

	int vBufferSize = sizeof(vList);

	// create default heap, which is memory in gpu that only gpu has access to
	device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vBufferSize),
		// copy dest since data will be copied from the upload heap to the default heap
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&vertexBuffer)
	);

	vertexBuffer->SetName(L"Vertex Buffer Resource Heap");

	// create upload heap which uploads data to gpu which cpu can write to and gpu can read from
	ID3D12Resource* vBufferUploadHeap;
	device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vBufferSize),
		// set as read because gpu will read from the buffer
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vBufferUploadHeap)
	);

	vBufferUploadHeap->SetName(L"Vertex Buffer Upload Resource Heap");

	// buffer contains one subresource
	D3D12_SUBRESOURCE_DATA vertexData = {};
	vertexData.pData = reinterpret_cast<BYTE*>(vList);
	// because the array is 1D, the row pitch is the same as the slicepitch
	// slicepitch is only different in 2D arrays
	vertexData.RowPitch = vBufferSize;
	vertexData.SlicePitch = vBufferSize;

	// update subresources in the vertexBuffer using the data in vBufferUploadHeap
	UpdateSubresources(commandList, vertexBuffer, vBufferUploadHeap, 0, 0, 1, &vertexData);

	// transition vertex buffer data to vertex buffer state (it started in copy destination state above)
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	// index list
	DWORD iList[] = {
		0, 1, 2,
		0, 3, 1,

		4, 5, 6, 
		4, 7, 5, 

		8, 9, 10, 
		8, 11, 9,

		12, 13, 14,
		12, 15, 13,

		16, 17, 18,
		16, 19, 17,

		20, 21, 22,
		20, 23, 21,
	};

	int iBufferSize = sizeof(iList);

	numCubeIndices = sizeof(iList) / sizeof(DWORD);

	device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(iBufferSize),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&indexBuffer)
	);

	indexBuffer->SetName(L"Index Buffer Resource Heap");

	ID3D12Resource* iBufferUploadHeap;
	device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vBufferSize),
		// set as read because gpu will read from the buffer
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&iBufferUploadHeap)
	);

	iBufferUploadHeap->SetName(L"Index Buffer Upload Resource Heap");

	D3D12_SUBRESOURCE_DATA indexData = {};
	indexData.pData = reinterpret_cast<BYTE*>(iList);
	indexData.RowPitch = iBufferSize;
	indexData.SlicePitch = iBufferSize;

	UpdateSubresources(commandList, indexBuffer, iBufferUploadHeap, 0, 0, 1, &indexData);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(indexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	// create depth/stencil buffer
	// Depth Stencil View
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	hr = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsDescriptorHeap));
	if (FAILED(hr))
		Running = false;

	D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
	depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

	D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
	depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
	depthOptimizedClearValue.DepthStencil.Stencil = 0;

	device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, Width, Height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthOptimizedClearValue,
		IID_PPV_ARGS(&depthStencilBuffer)
	);

	dsDescriptorHeap->SetName(L"Depth/Stencil Resource Heap");

	device->CreateDepthStencilView(depthStencilBuffer, &depthStencilDesc, dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart());


	for (int i = 0; i < frameBufferCount; ++i) {
		hr = device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64), // 64 KB multiple
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&constantBufferUploadHeaps[i])
		);
		constantBufferUploadHeaps[i]->SetName(L"Constant Buffer Upload Resource Heap");
	
		// for previous color data
		/*
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = constantBufferUploadHeaps[i]->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = (sizeof(ConstantBuffer) + 255) & ~255; // has to be 256 byte aligned?
		device->CreateConstantBufferView(&cbvDesc, mainDescriptorHeap[i]->GetCPUDescriptorHandleForHeapStart());
		
		ZeroMemory(&cbColorMultiplierData, sizeof(cbColorMultiplierData));
		*/

		ZeroMemory(&cbPerObject, sizeof(cbPerObject));

		// cpu will not read from constant buffer
		CD3DX12_RANGE readRange(0, 0);
		/*
		hr = constantBufferUploadHeaps[i]->Map(0, &readRange, reinterpret_cast<void**>(&cbColorMultiplierGPUAddress[i]));
		memcpy(cbColorMultiplierGPUAddress[i], &cbColorMultiplierData, sizeof(cbColorMultiplierData));
		*/
		hr = constantBufferUploadHeaps[i]->Map(0, &readRange, reinterpret_cast<void**>(&cbvGPUAddress[i]));

		// remember 256 bit alignments
		memcpy(cbvGPUAddress[i], &cbPerObject, sizeof(cbPerObject));
		memcpy(cbvGPUAddress[i] + ConstantBufferPerObjectAlignedSize, &cbPerObject, sizeof(cbPerObject));
	}

	D3D12_RESOURCE_DESC textureDesc;
	int imageBytesPerRow;
	BYTE* imageData;
	int imageSize = LoadImageDataFromFile(&imageData, textureDesc, L"braynzar.jpg", imageBytesPerRow);

	if (imageSize <= 0) {
		Running = false;
		return false;
	}

	hr = device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&textureBuffer)
	);
	if (FAILED(hr)) {
		Running = false;
		return false;
	}
	textureBuffer->SetName(L"Texture Buffer Resource Heap");

	UINT64 textureUploadBufferSize;
	// texture upload heap must be 256 byte aligned
	device->GetCopyableFootprints(&textureDesc, 0, 1, 0, nullptr, nullptr, nullptr, &textureUploadBufferSize);

	hr = device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(textureUploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&textureBufferUploadHeap)
	);
	if (FAILED(hr)) {
		Running = false;
		return false;
	}
	textureBufferUploadHeap->SetName(L"Texture Buffer Upload Resource Heap");

	D3D12_SUBRESOURCE_DATA textureData = {};
	textureData.pData = &imageData[0];
	textureData.RowPitch = imageBytesPerRow;
	textureData.SlicePitch = imageBytesPerRow * textureDesc.Height;

	UpdateSubresources(commandList, textureBuffer, textureBufferUploadHeap, 0, 0, 1, &textureData);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(textureBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

	// descriptor heaps
	for (int i = 0; i < frameBufferCount; ++i) {
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = 1;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mainDescriptorHeap[i]));
		if (FAILED(hr))
			Running = false;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		// 1 to 1 mapping
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = textureDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(textureBuffer, &srvDesc, mainDescriptorHeap[i]->GetCPUDescriptorHandleForHeapStart());
	}

	// execute command list
	commandList->Close();
	ID3D12CommandList* ppCommandLists[] = { commandList };
	commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// increment fence value to ensure data is uploaded before drawing
	fenceValue[frameIndex]++;
	hr = commandQueue->Signal(fence[frameIndex], fenceValue[frameIndex]);
	if (FAILED(hr)) {
		Running = false;
		return false;
	}

	delete imageData;

	vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	// stride is the total size of an array slot (can be bigger than an array element, which means extra space between elements)
	vertexBufferView.StrideInBytes = sizeof(Vertex);
	vertexBufferView.SizeInBytes = vBufferSize;

	indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	indexBufferView.SizeInBytes = iBufferSize;

	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = Width;
	viewport.Height = Height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	scissorRect.left = 0;
	scissorRect.top = 0;
	scissorRect.right = Width;
	scissorRect.bottom = Height;

	DirectX::XMMATRIX tmpMat = DirectX::XMMatrixPerspectiveFovLH(45.0f * (3.14f / 180.0f), (float)Width / (float)Height, 0.1f, 1000.0f);
	DirectX::XMStoreFloat4x4(&cameraProjMat, tmpMat);

	cameraPosition = DirectX::XMFLOAT4(0.0f, 2.0f, -4.0f, 0.0f);
	cameraTarget = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	cameraUp = DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);

	DirectX::XMVECTOR cPos = DirectX::XMLoadFloat4(&cameraPosition);
	DirectX::XMVECTOR cTarg = DirectX::XMLoadFloat4(&cameraTarget);
	DirectX::XMVECTOR cUp = DirectX::XMLoadFloat4(&cameraUp);
	// look at lh is an easy way to get the view matrix
	tmpMat = DirectX::XMMatrixLookAtLH(cPos, cTarg, cUp);
	DirectX::XMStoreFloat4x4(&cameraViewMat, tmpMat);

	cube1Position = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	DirectX::XMVECTOR posVec = XMLoadFloat4(&cube1Position);

	tmpMat = DirectX::XMMatrixTranslationFromVector(posVec);
	DirectX::XMStoreFloat4x4(&cube1RotMat, DirectX::XMMatrixIdentity());
	DirectX::XMStoreFloat4x4(&cube1WorldMat, tmpMat);

	cube2PositionOffset = DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f);
	posVec = DirectX::XMLoadFloat4(&cube2PositionOffset) + DirectX::XMLoadFloat4(&cube1Position);

	tmpMat = DirectX::XMMatrixTranslationFromVector(posVec);
	DirectX::XMStoreFloat4x4(&cube2RotMat, DirectX::XMMatrixIdentity());
	DirectX::XMStoreFloat4x4(&cube2WorldMat, tmpMat);

	return true;
}

// update game logic
void Update() {
	DirectX::XMFLOAT4 upVector = DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
	DirectX::XMVECTOR rotAxis = DirectX::XMLoadFloat4(&upVector);

	DirectX::XMMATRIX rotMat = DirectX::XMLoadFloat4x4(&cube1RotMat) * DirectX::XMMatrixRotationAxis(rotAxis, 0.0001f);
	DirectX::XMStoreFloat4x4(&cube1RotMat, rotMat);

	DirectX::XMMATRIX translationMat = DirectX::XMMatrixTranslationFromVector(XMLoadFloat4(&cube1Position));
	DirectX::XMMATRIX worldMat = rotMat * translationMat;
	DirectX::XMStoreFloat4x4(&cube1WorldMat, worldMat);

	DirectX::XMMATRIX viewMat = DirectX::XMLoadFloat4x4(&cameraViewMat);
	DirectX::XMMATRIX projMat = DirectX::XMLoadFloat4x4(&cameraProjMat);
	DirectX::XMMATRIX wvpMat = DirectX::XMLoadFloat4x4(&cube1WorldMat) * viewMat * projMat;
	// transposed because DirectX math library is row major, not column major
	DirectX::XMMATRIX transposed = DirectX::XMMatrixTranspose(wvpMat);
	DirectX::XMStoreFloat4x4(&cbPerObject.wvpMat, transposed);

	memcpy(cbvGPUAddress[frameIndex], &cbPerObject, sizeof(cbPerObject));

	// cube 2
	DirectX::XMStoreFloat4x4(&cube2RotMat, rotMat);
	DirectX::XMMATRIX translationOffsetMat = DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat4(&cube2PositionOffset));

	DirectX::XMMATRIX scaleMat = DirectX::XMMatrixScaling(0.5f, 0.5f, 0.5f);
	worldMat = scaleMat * translationOffsetMat * rotMat * translationMat;

	wvpMat = XMLoadFloat4x4(&cube2WorldMat) * viewMat * projMat; 
	transposed = XMMatrixTranspose(wvpMat);
	XMStoreFloat4x4(&cbPerObject.wvpMat, transposed);

	memcpy(cbvGPUAddress[frameIndex] + ConstantBufferPerObjectAlignedSize, &cbPerObject, sizeof(cbPerObject));

	XMStoreFloat4x4(&cube2WorldMat, worldMat);
}

void UpdatePipeline() {
	HRESULT hr;

	WaitForPreviousFrame();

	hr = commandAllocator[frameIndex]->Reset();
	if (FAILED(hr))
		Running = false;

	// resetting allows commands to start being recorded
	hr = commandList->Reset(commandAllocator[frameIndex], pipelineStateObject);
	if (FAILED(hr))
		Running = false;

	// recording commands
	// note that doing something bad during recording does not stop program from running (dx12)

	// resource barrier changes the resource state to a render target state in order to change the 
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorSize);

	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	// Output Merger
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	// clear depth buffer from last frame
	commandList->ClearDepthStencilView(dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// draw triangles
	commandList->SetGraphicsRootSignature(rootSignature);

	ID3D12DescriptorHeap* descriptorHeaps[] = { mainDescriptorHeap[frameIndex] };
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	commandList->SetGraphicsRootDescriptorTable(1, mainDescriptorHeap[frameIndex]->GetGPUDescriptorHandleForHeapStart());

	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
	commandList->IASetIndexBuffer(&indexBufferView);

	// draw cube 1
	commandList->SetGraphicsRootConstantBufferView(0, constantBufferUploadHeaps[frameIndex]->GetGPUVirtualAddress());

	commandList->DrawIndexedInstanced(numCubeIndices, 1, 0, 0, 0);

	// draw cube 2
	commandList->SetGraphicsRootConstantBufferView(0, constantBufferUploadHeaps[frameIndex]->GetGPUVirtualAddress() + ConstantBufferPerObjectAlignedSize);

	commandList->DrawIndexedInstanced(numCubeIndices, 1, 0, 0, 0);

	// transition back
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	hr = commandList->Close();
	if (FAILED(hr))
		Running = false;
}

void Render() {
	HRESULT hr;

	// sends commands to command queue
	UpdatePipeline();

	// if multithreaded, one command list per thread
	ID3D12CommandList* ppCommandLists[] = { commandList };

	commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// sets fence and signals fence event so that when coming back to the frame buffer, we can see whether or not GPU has finished executing list
	// since signal command would have executed and fence would be set to the value
	hr = commandQueue->Signal(fence[frameIndex], fenceValue[frameIndex]);
	if (FAILED(hr))
		Running = false;

	// present back buffer
	hr = swapChain->Present(0, 0);
	if (FAILED(hr))
		Running = false;
}

void Cleanup() {
	// finish going through frames
	for (int i = 0; i < frameBufferCount; ++i) {
		frameIndex = i;
		WaitForPreviousFrame();
	}

	BOOL fs = false;
	if (swapChain->GetFullscreenState(&fs, NULL))
		swapChain->SetFullscreenState(false, NULL);

	SAFE_RELEASE(device);
	SAFE_RELEASE(swapChain);
	SAFE_RELEASE(commandQueue);
	SAFE_RELEASE(rtvDescriptorHeap);
	SAFE_RELEASE(commandList);

	for (int i = 0; i < frameBufferCount; ++i) {
		SAFE_RELEASE(renderTargets[i]);
		SAFE_RELEASE(commandAllocator[i]);
		SAFE_RELEASE(fence[i]);

		SAFE_RELEASE(mainDescriptorHeap[i]);
		//SAFE_RELEASE(constantBufferUploadHeap[i]);
	
		SAFE_RELEASE(constantBufferUploadHeaps[i]);
		
	}

	SAFE_RELEASE(pipelineStateObject);
	SAFE_RELEASE(rootSignature);
	SAFE_RELEASE(vertexBuffer);

	SAFE_RELEASE(indexBuffer);
}

void WaitForPreviousFrame() {
	HRESULT hr;

	frameIndex = swapChain->GetCurrentBackBufferIndex();

	// if wanted fence value is less than the current fence value
	if (fence[frameIndex]->GetCompletedValue() < fenceValue[frameIndex]) {
		// set fence event that will be triggered once the value of the first parameter is met
		hr = fence[frameIndex]->SetEventOnCompletion(fenceValue[frameIndex], fenceEvent);
		if (FAILED(hr))
			Running = false;

		// wait until fence has triggered event
		WaitForSingleObject(fenceEvent, INFINITE);
	}

	++fenceValue[frameIndex];
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
	if (!InitializeWindow(hInstance, nShowCmd, FullScreen)) {
		MessageBox(0, L"Window Initialization - Failed", L"Error", MB_OK);
		return 1;
	}

	if (!InitD3D()) {
		MessageBox(0, L"Failed to initialize Direct3D 12", L"Error", MB_OK);
		Cleanup();
		return 1;
	}

	mainloop();

	WaitForPreviousFrame();

	CloseHandle(fenceEvent);

	Cleanup();
	
	return 0;
}