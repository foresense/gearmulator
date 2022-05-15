#include "controller.h"

#include <cassert>

#include "parameter.h"

namespace pluginLib
{
	Controller::Controller(const std::string& _parameterDescJson) : m_descriptions(_parameterDescJson)
	{
	}
	
	void Controller::registerParams(juce::AudioProcessor& _processor)
    {
		auto globalParams = std::make_unique<juce::AudioProcessorParameterGroup>("global", "Global", "|");

		std::map<ParamIndex, int> knownParameterIndices;

    	for (uint8_t part = 0; part < 16; part++)
		{
			m_paramsByParamType[part].reserve(m_descriptions.getDescriptions().size());

    		const auto partNumber = juce::String(part + 1);
			auto group =
				std::make_unique<juce::AudioProcessorParameterGroup>("ch" + partNumber, "Ch " + partNumber, "|");

			uint32_t parameterDescIndex = 0;
			for (const auto& desc : m_descriptions.getDescriptions())
			{
				++parameterDescIndex;

				const ParamIndex idx = {static_cast<uint8_t>(desc.page), part, desc.index};

				int uid = 0;

				auto itKnownParamIdx = knownParameterIndices.find(idx);

				if(itKnownParamIdx == knownParameterIndices.end())
					knownParameterIndices.insert(std::make_pair(idx, 0));
				else
					uid = ++itKnownParamIdx->second;

				std::unique_ptr<Parameter> p;
				p.reset(createParameter(*this, desc, part, uid));

				if(uid > 0)
				{
					const auto& existingParams = findSynthParam(idx);

					for (auto& existingParam : existingParams)
						existingParam->addLinkedParameter(p.get());
				}

				m_paramsByParamType[part].push_back(p.get());

				const bool isNonPartExclusive = (desc.classFlags & (int)pluginLib::ParameterClass::Global) || (desc.classFlags & (int)pluginLib::ParameterClass::NonPartSensitive);
				if (isNonPartExclusive)
				{
					if (part != 0)
						continue; // only register on first part!
				}
				if (p->getDescription().isPublic)
				{
					// lifecycle managed by Juce

					auto itExisting = m_synthParams.find(idx);
					if (itExisting != m_synthParams.end())
					{
						itExisting->second.push_back(p.get());
					}
					else
					{
						ParameterList params;
						params.emplace_back(p.get());
						m_synthParams.insert(std::make_pair(idx, std::move(params)));
					}

					if (isNonPartExclusive)
					{
						jassert(part == 0);
						globalParams->addChild(std::move(p));
					}
					else
						group->addChild(std::move(p));
				}
				else
				{
					// lifecycle handled by us

					auto itExisting = m_synthInternalParams.find(idx);
					if (itExisting != m_synthInternalParams.end())
					{
						itExisting->second.push_back(p.get());
					}
					else
					{
						ParameterList params;
						params.emplace_back(p.get());
						m_synthInternalParams.insert(std::make_pair(idx, std::move(params)));
					}
					m_synthInternalParamList.emplace_back(std::move(p));
				}
			}
			_processor.addParameterGroup(std::move(group));
		}
		_processor.addParameterGroup(std::move(globalParams));
	}

	const Controller::ParameterList& Controller::findSynthParam(const uint8_t _part, const uint8_t _page, const uint8_t _paramIndex)
	{
		const ParamIndex paramIndex{ _page, _part, _paramIndex };

		return findSynthParam(paramIndex);
	}

	const Controller::ParameterList& Controller::findSynthParam(const ParamIndex& _paramIndex)
    {
		const auto it = m_synthParams.find(_paramIndex);

		if (it != m_synthParams.end())
			return it->second;

    	const auto iti = m_synthInternalParams.find(_paramIndex);

		if (iti == m_synthInternalParams.end())
		{
			static ParameterList empty;
			return empty;
		}

		return iti->second;
    }

    juce::Value* Controller::getParamValueObject(const uint32_t _index)
    {
	    const auto res = getParameter(_index);
		return res ? &res->getValueObject() : nullptr;
    }

    Parameter* Controller::getParameter(const uint32_t _index) const
    {
		return getParameter(_index, 0);
	}

	Parameter* Controller::getParameter(const uint32_t _index, const uint8_t _part) const
	{
		if (_part >= m_paramsByParamType.size())
			return nullptr;

		if (_index >= m_paramsByParamType[_part].size())
			return nullptr;

		return m_paramsByParamType[_part][_index];
	}

	uint32_t Controller::getParameterIndexByName(const std::string& _name) const
	{
		uint32_t index;
		return m_descriptions.getIndexByName(index, _name) ? index : InvalidParameterIndex;
	}

	const MidiPacket* Controller::getMidiPacket(const std::string& _name) const
	{
		return m_descriptions.getMidiPacket(_name);
	}

	bool Controller::createMidiDataFromPacket(std::vector<uint8_t>& _sysex, const std::string& _packetName, const std::map<MidiDataType, uint8_t>& _params) const
	{
        const auto* m = getMidiPacket(_packetName);
		assert(m && "midi packet not found");
        if(!m)
            return false;

    	if(!m->create(_sysex, _params))
    	{
	        assert(false && "failed to create midi packet");
			_sysex.clear();
            return false;
    	}
		return true;
	}
}