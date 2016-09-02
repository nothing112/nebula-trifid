//------------------------------------------------------------------------------
// vkpass.cc
// (C) 2016 Individual contributors, see AUTHORS file
//------------------------------------------------------------------------------
#include "stdneb.h"
#include "vkpass.h"
#include "vkrenderdevice.h"
#include "vktypes.h"
#include "coregraphics/rendertexture.h"
#include "coregraphics/config.h"
#include "coregraphics/shaderserver.h"

using namespace CoreGraphics;
namespace Vulkan
{

__ImplementClass(Vulkan::VkPass, 'VKFR', Base::PassBase);
//------------------------------------------------------------------------------
/**
*/
VkPass::VkPass()
{
	// empty
}

//------------------------------------------------------------------------------
/**
*/
VkPass::~VkPass()
{
	// empty
}

//------------------------------------------------------------------------------
/**
*/
void
VkPass::Setup()
{
	// setup base class
	PassBase::Setup();

	// create shader state for input attachments and render target dimensions
	this->shaderState = ShaderServer::Instance()->CreateShaderState("shd:shared", { NEBULAT_PASS_GROUP });
	VkDescriptorSetLayout layout = this->shaderState->GetShader()->GetDescriptorLayout(NEBULAT_PASS_GROUP);

	// create descriptor set used by our pass
	VkDescriptorSetAllocateInfo descInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		NULL,
		VkRenderDevice::descPool,
		1,
		&layout
	};
	VkResult res = vkAllocateDescriptorSets(VkRenderDevice::dev, &descInfo, &this->inputAttachmentDescriptorSet);
	n_assert(res == VK_SUCCESS);

	Util::FixedArray<VkSubpassDescription> subpassDescs;
	Util::FixedArray<Util::FixedArray<VkAttachmentReference>> subpassReferences;
	Util::FixedArray<Util::FixedArray<VkAttachmentReference>> subpassInputs;
	Util::FixedArray<Util::FixedArray<uint32_t>> subpassPreserves;
	Util::FixedArray<Util::FixedArray<VkAttachmentReference>> subpassResolves;
	Util::FixedArray<VkAttachmentReference> subpassDepthStencils;
	Util::Array<VkSubpassDependency> subpassDeps;

	// resize subpass contents
	subpassDescs.Resize(this->subpasses.Size());
	subpassReferences.Resize(this->subpasses.Size());
	subpassInputs.Resize(this->subpasses.Size());
	subpassPreserves.Resize(this->subpasses.Size());
	subpassResolves.Resize(this->subpasses.Size());
	subpassDepthStencils.Resize(this->subpasses.Size());

	IndexT i;
	for (i = 0; i < this->subpasses.Size(); i++)
	{
		const PassBase::Subpass& subpass = this->subpasses[i];

		VkSubpassDescription& vksubpass = subpassDescs[i];
		vksubpass.flags = 0;
		vksubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		vksubpass.pColorAttachments = NULL;
		vksubpass.pDepthStencilAttachment = NULL;
		vksubpass.pPreserveAttachments = NULL;
		vksubpass.pResolveAttachments = NULL;
		vksubpass.pInputAttachments = NULL;

		// get references to fixed arrays
		Util::FixedArray<VkAttachmentReference>& references = subpassReferences[i];
		Util::FixedArray<VkAttachmentReference>& inputs = subpassInputs[i];
		Util::FixedArray<uint32_t>& preserves = subpassPreserves[i];
		Util::FixedArray<VkAttachmentReference>& resolves = subpassResolves[i];

		// resize arrays straight away since we already know the size
		references.Resize(this->colorAttachments.Size());
		inputs.Resize(subpass.inputs.Size());
		preserves.Resize(this->colorAttachments.Size() - subpass.attachments.Size());
		if (subpass.resolve) resolves.Resize(subpass.attachments.Size());

		// if subpass binds depth, the slot for the depth-stencil buffer is color attachments + 1
		if (subpass.bindDepth)
		{
			VkAttachmentReference& ds = subpassDepthStencils[i];
			ds.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			ds.attachment = this->colorAttachments.Size();
			vksubpass.pDepthStencilAttachment = &ds;
		}
		else
		{
			VkAttachmentReference& ds = subpassDepthStencils[i];
			ds.layout = VK_IMAGE_LAYOUT_UNDEFINED;
			ds.attachment = VK_ATTACHMENT_UNUSED;
			vksubpass.pDepthStencilAttachment = &ds;
		}

		IndexT idx = 0;
		SizeT boundAttachments = 0;
		SizeT preserveAttachments = 0;
		IndexT j;
		for (j = 0; j < this->colorAttachments.Size(); j++)
		{
			// if we can find the attachment in the subpass, use it, otherwise bind it as unused
			IndexT foundIndex = subpass.attachments.FindIndex(j);
			if (foundIndex != InvalidIndex)
			{
				VkAttachmentReference& ref = references[j];
				ref.attachment = subpass.attachments[foundIndex];
				ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				if (subpass.resolve) resolves[j] = ref;
			}
			else
			{
				VkAttachmentReference& ref = references[j];
				ref.attachment = VK_ATTACHMENT_UNUSED;
				ref.layout = VK_IMAGE_LAYOUT_UNDEFINED;
				preserves[preserveAttachments++] = j;
			}			
		}

		for (j = 0; j < subpass.inputs.Size(); j++)
		{
			VkAttachmentReference& ref = inputs[j];
			ref.attachment = subpass.inputs[j];
			ref.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}

		for (j = 0; j < subpass.dependencies.Size(); j++)
		{
			VkSubpassDependency dep;
			dep.srcSubpass = i;
			dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dep.dstSubpass = subpass.dependencies[j];
			dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
			dep.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			subpassDeps.Append(dep);
		}
		
		// set color attachments
		vksubpass.colorAttachmentCount = references.Size();
		vksubpass.pColorAttachments = references.Begin();

		// if we have subpass inputs, use them
		if (inputs.Size() > 0)
		{
			vksubpass.inputAttachmentCount = inputs.Size();
			vksubpass.pInputAttachments = inputs.Begin();
		}
		else
		{
			vksubpass.inputAttachmentCount = 0;
		}
		
		// the rest are automatically preserve
		if (preserves.Size() > 0)
		{ 
			vksubpass.preserveAttachmentCount = preserves.Size();
			vksubpass.pPreserveAttachments = preserves.Begin();
		}
		else
		{
			vksubpass.preserveAttachmentCount = 0;
		}
	}

	VkAttachmentLoadOp loadOps[] =
	{
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_LOAD_OP_LOAD,
	};

	VkAttachmentStoreOp storeOps[] =
	{
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_STORE,
	};

	SizeT numUsedAttachments = this->colorAttachments.Size();
	Util::FixedArray<VkAttachmentDescription> attachments;
	attachments.Resize(this->colorAttachments.Size() + 1);
	for (i = 0; i < this->colorAttachments.Size(); i++)
	{
		VkFormat fmt = VkTypes::AsVkFormat(this->colorAttachments[i]->GetPixelFormat());
		VkAttachmentDescription& attachment = attachments[i];
		IndexT loadIdx = this->colorAttachmentFlags[i] & Load ? 2 : this->colorAttachmentFlags[i] & Clear ? 1 : 0;
		IndexT storeIdx = this->colorAttachmentFlags[i] & Store ? 1 : 0;
		attachment.flags = 0;
		attachment.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachment.format = fmt;
		attachment.loadOp = loadOps[loadIdx];
		attachment.storeOp = storeOps[storeIdx];
		attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.samples = this->colorAttachments[i]->GetEnableMSAA() ? VK_SAMPLE_COUNT_16_BIT : VK_SAMPLE_COUNT_1_BIT;
	}

	// use depth stencil attachments if pointer is not null
	if (this->depthStencilAttachment.isvalid())
	{
		VkAttachmentDescription& attachment = attachments[i];
		IndexT loadIdx = this->depthStencilFlags & Load ? 2 : this->depthStencilFlags & Clear ? 1 : 0;
		IndexT storeIdx = this->depthStencilFlags & Store ? 1 : 0;
		IndexT stencilLoadIdx = this->depthStencilFlags & LoadStencil ? 2 : this->depthStencilFlags & ClearStencil ? 1 : 0;
		IndexT stencilStoreIdx = this->depthStencilFlags & StoreStencil ? 1 : 0;
		attachment.flags = 0;
		attachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		attachment.format = VkTypes::AsVkFormat(this->depthStencilAttachment->GetPixelFormat());
		attachment.loadOp = loadOps[loadIdx];
		attachment.storeOp = storeOps[storeIdx];
		attachment.stencilLoadOp = loadOps[stencilLoadIdx];
		attachment.stencilStoreOp = storeOps[stencilStoreIdx];
		attachment.samples = this->depthStencilAttachment->GetEnableMSAA() ? VK_SAMPLE_COUNT_16_BIT : VK_SAMPLE_COUNT_1_BIT;
		numUsedAttachments++;
	}
	
	// create render pass
	VkRenderPassCreateInfo info =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		NULL,
		0,
		numUsedAttachments,
		attachments.Begin(),
		subpassDescs.Size(),
		subpassDescs.Begin(),
		subpassDeps.Size(),
		subpassDeps.Begin()
	};
	res = vkCreateRenderPass(VkRenderDevice::dev, &info, NULL, &this->pass);
	n_assert(res == VK_SUCCESS);

	// gather image views
	SizeT width = 0;
	SizeT height = 0;
	SizeT layers = 0;
	Util::FixedArray<VkImageView> images;
	images.Resize(this->colorAttachments.Size() + (this->depthStencilAttachment.isvalid() ? 1 : 0));
	this->scissorRects.Resize(images.Size());
	this->viewports.Resize(images.Size());
	for (i = 0; i < this->colorAttachments.Size(); i++)
	{
		images[i] = this->colorAttachments[i]->GetVkImageView();
		width = Math::n_max(width, this->colorAttachments[i]->GetWidth());
		height = Math::n_max(height, this->colorAttachments[i]->GetHeight());
		layers = Math::n_max(layers, this->colorAttachments[i]->GetDepth());

		VkRect2D& rect = scissorRects[i];
		rect.offset.x = 0;
		rect.offset.y = 0;
		rect.extent.width = this->colorAttachments[i]->GetWidth();
		rect.extent.height = this->colorAttachments[i]->GetHeight();
		VkViewport& viewport = viewports[i];
		viewport.width = (float)this->colorAttachments[i]->GetWidth();
		viewport.height = (float)this->colorAttachments[i]->GetHeight();
		viewport.minDepth = 0;
		viewport.maxDepth = 1;
		viewport.x = 0;
		viewport.y = 0;
		
	}
	if (this->depthStencilAttachment.isvalid()) images[i] = this->depthStencilAttachment->GetVkImageView();

	// setup viewport info
	this->viewportInfo.pNext = NULL;
	this->viewportInfo.flags = 0;
	this->viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	this->viewportInfo.scissorCount = this->scissorRects.Size();
	this->viewportInfo.pScissors = this->scissorRects.Begin();
	this->viewportInfo.viewportCount = this->viewports.Size();
	this->viewportInfo.pViewports = this->viewports.Begin();

	// update descriptor set based on images attachments
	for (i = 0; i < images.Size(); i++)
	{
		VkWriteDescriptorSet write;
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.pNext = NULL;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		write.dstBinding = 0;
		write.dstArrayElement = i;
		write.dstSet = this->inputAttachmentDescriptorSet;

		VkDescriptorImageInfo img;
		img.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		img.imageView = images[i];
		img.sampler = VK_NULL_HANDLE;
		write.pImageInfo = &img;

		// update descriptor set with attachment
		vkUpdateDescriptorSets(VkRenderDevice::dev, 1, &write, 0, NULL);
	}
	this->shaderState->SetDescriptorSet(this->inputAttachmentDescriptorSet, NEBULAT_PASS_GROUP);

	// create framebuffer
	VkFramebufferCreateInfo fbInfo =
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		NULL,
		0,
		this->pass,
		images.Size(),
		images.Begin(),
		width,
		height,
		layers
	};
	res = vkCreateFramebuffer(VkRenderDevice::dev, &fbInfo, NULL, &this->framebuffer);
	n_assert(res == VK_SUCCESS);

	// setup info
	this->framebufferPipelineInfo.renderPass = this->pass;
	this->framebufferPipelineInfo.subpass = 0;
	this->framebufferPipelineInfo.pViewportState = &this->viewportInfo;
}

//------------------------------------------------------------------------------
/**
*/
void
VkPass::Discard()
{
	vkDestroyRenderPass(VkRenderDevice::dev, this->pass, NULL);
	vkDestroyFramebuffer(VkRenderDevice::dev, this->framebuffer, NULL);
}

//------------------------------------------------------------------------------
/**
*/
void
VkPass::Begin()
{
	PassBase::Begin();

	// commit this shader state
	this->shaderState->Commit();

	// update framebuffer pipeline info to next subpass
	this->framebufferPipelineInfo.subpass = this->currentSubpass;
}

//------------------------------------------------------------------------------
/**
*/
void
VkPass::NextSubpass()
{
	PassBase::NextSubpass();
	this->framebufferPipelineInfo.subpass = this->currentSubpass;
}

//------------------------------------------------------------------------------
/**
*/
void
VkPass::End()
{
	PassBase::End();
}

} // namespace Vulkan