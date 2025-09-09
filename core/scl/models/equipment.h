#pragma once
#include <QString>

class Equipment {
public:
    Equipment() = default;

    QString getName() const;
    void setName(const QString& name);

    QString getType() const;
    void setType(const QString& type);

    QString getIedRef() const;
    void setIedRef(const QString& iedRef);

private:
    QString m_name;
    QString m_type;    // disjoncteur, sectionneur, transformateur, etc.
    QString m_iedRef;  // référence vers l’IED associé
};
