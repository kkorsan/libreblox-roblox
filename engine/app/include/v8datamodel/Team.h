#pragma once

#include "v8tree/Service.h"
#include "v8tree/Instance.h"
#include "util/Color.h"
#include "util/BrickColor.h"


namespace RBX {

	extern const char *const sTeam;
	class Team
		: public DescribedCreatable<Team, Instance, sTeam>
	{
	protected:
		int score;
		BrickColor color;
		bool autoAssignable;
		bool autoColorCharacters;

	public:
		Team();
		~Team();

		int getScore() const;
		void setScore(int score);
		BrickColor getTeamColor() const;
		void setTeamColor(BrickColor color);
		bool getAutoAssignable() const;
		void setAutoAssignable(bool autoAssign);

		static Reflection::BoundProp<bool> prop_AutoColorCharacters;
	protected:
		/* override */ bool askSetParent(const Instance* parent) const;
		/* override */ bool askAddChild(const Instance* instance) const {return true;}

	};
}
