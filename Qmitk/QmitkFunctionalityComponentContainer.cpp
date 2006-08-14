/*=========================================================================

Program:   Medical Imaging & Interaction Toolkit
Module:    $RCSfile$
Language:  C++
Date:      $Date$
Version:   $Revision$ 

Copyright (c) German Cancer Research Center, Division of Medical and
Biological Informatics. All rights reserved.
See MITKCopyright.txt or http://www.mitk.org/copyright.html for details.

This software is distributed WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "QmitkFunctionalityComponentContainer.h"
#include "QmitkFunctionalityComponentContainerGUI.h"

#include <QmitkDataTreeComboBox.h>
#include <mitkDataTreeFilter.h>

/*****Qt-Elements***/
#include <qgroupbox.h>
#include <qlayout.h>
#include <itkCommand.h>
#include <qobjectlist.h>
#include <qcombobox.h>

const QSizePolicy preferred(QSizePolicy::Preferred, QSizePolicy::Preferred);

/***************       CONSTRUCTOR      ***************/
QmitkFunctionalityComponentContainer::QmitkFunctionalityComponentContainer(QObject *parent, const char *name, mitk::DataTreeIteratorBase* it)
: QmitkBaseFunctionalityComponent(name, it),m_Parent(parent), m_GUI(NULL),m_Name(name), m_SelectedImage(NULL)
{
  SetDataTreeIterator(it);

}

/***************        DESTRUCTOR      ***************/
QmitkFunctionalityComponentContainer::~QmitkFunctionalityComponentContainer()
{
  if(m_DataTreeIterator.IsNotNull() )
  {
    m_DataTreeIterator->GetTree()->RemoveObserver(m_ObserverTag);
  }
}

/*************** GET FUNCTIONALITY NAME ***************/
QString QmitkFunctionalityComponentContainer::GetFunctionalityName()
{
  return m_Name;
}

/*************** SET FUNCTIONALITY NAME ***************/
void QmitkFunctionalityComponentContainer::SetFunctionalityName(QString name)
{
  m_Name = name;
}

/*************** GET FUNCCOMPONENT NAME ***************/
QString QmitkFunctionalityComponentContainer::GetFunctionalityComponentName()
{
  return m_Name;
}

/*************** GET DATA TREE ITERATOR ***************/
mitk::DataTreeIteratorBase* QmitkFunctionalityComponentContainer::GetDataTreeIterator()
{
  return m_DataTreeIterator.GetPointer();
}

/*************** SET DATA TREE ITERATOR ***************/
void QmitkFunctionalityComponentContainer::SetDataTreeIterator(mitk::DataTreeIteratorBase* it)
{
  if(m_DataTreeIterator.IsNotNull() )
  {
    m_DataTreeIterator->GetTree()->RemoveObserver(m_ObserverTag);
  }
  m_DataTreeIterator = it;
  if(m_DataTreeIterator.IsNotNull())
  {
    itk::ReceptorMemberCommand<QmitkFunctionalityComponentContainer>::Pointer command = itk::ReceptorMemberCommand<QmitkFunctionalityComponentContainer>::New();
    command->SetCallbackFunction(this, &QmitkFunctionalityComponentContainer::TreeChanged);
    m_ObserverTag = m_DataTreeIterator->GetTree()->AddObserver(itk::TreeChangeEvent<mitk::DataTreeBase>(), command);
  }
}

/***************         GET GUI        ***************/
QWidget* QmitkFunctionalityComponentContainer::GetGUI()
{
  return m_GUI;
}

/*************** TREE CHANGED ( EVENT ) ***************/
void QmitkFunctionalityComponentContainer::TreeChanged(const itk::EventObject & /*treeChangedEvent*/)
{
  if(IsActivated())
  {
    m_TreeChangedWhileInActive = false;
    TreeChanged();
  }
  else
    m_TreeChangedWhileInActive = true;
  TreeChanged();
}

/*************** TREE CHANGED (       ) ***************/
void QmitkFunctionalityComponentContainer::TreeChanged()
{
  //if(m_SelectedImage != NULL)
    //SetDataTreeIterator(m_SelectedImage);
  if(m_GUI != NULL)
  {
    for(unsigned int i = 0;  i < m_Qbfc.size(); i++)
    {
      if(m_SelectedImage != NULL)
      {
       m_Qbfc[i]->ImageSelected(m_SelectedImage);
      }
    }
  }
}

/*************** TREE CHANGED(ITERATOR) ***************/
void QmitkFunctionalityComponentContainer::TreeChanged(mitk::DataTreeIteratorBase* it)
{
  if(m_GUI != NULL)
  {
    //SetDataTreeIterator(it);
//    m_GUI->GetTreeNodeSelector()->SetDataTreeNodeIterator(this->GetDataTreeIterator());
//    m_GUI->GetTreeNodeSelector()->UpdateContent();
    for(unsigned int i = 0;  i < m_Qbfc.size(); i++)
    {
      m_Qbfc[i]->ImageSelected(m_SelectedImage);
      //m_Qbfc[i]->UpdateTreeNodeSelector(this->GetDataTreeIterator());
      //m_Qbfc[i]->UpdateSelector(m_GUI->GetTreeNodeSelector()->m_ComboBox->currentText());
    }

//        QString name = m_GUI->GetTreeNodeSelector()->m_ComboBox->currentText();
     
  }
}

/***************       CONNECTIONS      ***************/
void QmitkFunctionalityComponentContainer::CreateConnections()
{
  if ( m_GUI )
  {
    connect( (QObject*)(m_GUI->GetTreeNodeSelector()), SIGNAL(activated(const mitk::DataTreeFilter::Item *)), (QObject*) this, SLOT(ImageSelected(const mitk::DataTreeFilter::Item *)));
  }
}

/***************     IMAGE SELECTED     ***************/
void QmitkFunctionalityComponentContainer::ImageSelected(const mitk::DataTreeFilter::Item * imageIt)
{
  m_SelectedImage = imageIt;
  TreeChanged();
}

/*************** CREATE CONTAINER WIDGET **************/
QWidget* QmitkFunctionalityComponentContainer::CreateContainerWidget(QWidget* parent)
{
  if (m_GUI == NULL)
  {
    m_GUI = new QmitkFunctionalityComponentContainerGUI(parent);
    m_GUI->GetTreeNodeSelector()->SetDataTree(GetDataTreeIterator());
    m_GUI->GetCompContBox()->setTitle("Container");
  }
  return m_GUI;
}

/***************        ACTIVATED       ***************/
void QmitkFunctionalityComponentContainer::Activated()
{
  m_Activated = true;
  if(m_TreeChangedWhileInActive)
  {
    TreeChanged();
    m_TreeChangedWhileInActive = false;
  }
}

/***************      ADD COMPONENT     ***************/
void QmitkFunctionalityComponentContainer::AddComponent(QmitkFunctionalityComponentContainer* componentContainer)
{  
  if(componentContainer!=NULL)
  {
    //TODO wieder reinholen
    //QWidget* componentWidget = componentContainer->CreateContainerWidget(m_GUI);
    m_Qbfc.push_back(componentContainer);
    //componentContainer->GetGUI()->setSizePolicy(preferred);

    QObjectList* list;
    list = m_GUI->queryList(0, "", true, true);
    uint NumOfChilds = list->count();
    list->insert(NumOfChilds, componentContainer->CreateContainerWidget(m_GUI));
    componentContainer->GetGUI()->setSizePolicy(preferred);
    //QLayout* containerLayout = m_GUI->GetContainerLayout();
    //m_GUI->insertChild(componentContainer->GetGUI());

    //uint numOfChilds = m_GUI->children()->count();
    //for(int i = 0; i<= numOfChilds; i++)
    //{
    //  componentContainer->GetGUI()->lower();
    //}

    m_GUI->setGeometry(componentContainer->GetGUI()->height(), 0, m_GUI->width(), m_GUI->height());
    //m_GUI->setGeometry(0, 0, m_GUI->width(), m_GUI->height());
    m_GUI->setSizePolicy(preferred);
    m_GUI->layout()->activate();

  }

}



